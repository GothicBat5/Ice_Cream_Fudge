package caddycmd

import (
	"bufio"
	"bytes"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"log"
	"log/slog"
	"net"
	"os"
	"path/filepath"
	"runtime"
	"runtime/debug"
	"strconv"
	"strings"
	"time"

	"github.com/KimMachineGun/automemlimit/memlimit"
	"github.com/caddyserver/certmagic"
	"github.com/spf13/pflag"
	"go.uber.org/automaxprocs/maxprocs"
	"go.uber.org/zap"
	"go.uber.org/zap/exp/zapslog"
	"github.com/caddyserver/caddy/v2"
	"github.com/caddyserver/caddy/v2/caddyconfig"
)

func init() {
	// set a fitting User-Agent for ACME requests
	version, _ := caddy.Version()
	cleanModVersion := strings.TrimPrefix(version, "v")
	ua := "Caddy/" + cleanModVersion
	if uaEnv, ok := os.LookupEnv("USERAGENT"); ok {
		ua = uaEnv + " " + ua
	}
	certmagic.UserAgent = ua
	certmagic.DefaultACME.Agreed = true
}

func Main() {
	if len(os.Args) == 0 {
		fmt.Printf("[FATAL] no arguments provided by OS; args[0] must be command\n")
		os.Exit(caddy.ExitCodeFailedStartup)
	}

	if err := defaultFactory.Build().Execute(); err != nil {
		var exitError *exitError
		if errors.As(err, &exitError) {
			os.Exit(exitError.ExitCode)
		}
		os.Exit(1)
	}
}

func handlePingbackConn(conn net.Conn, expect []byte) error {
	defer conn.Close()
	confirmationBytes, err := io.ReadAll(io.LimitReader(conn, 32))
	if err != nil {
		return err
	}
	if !bytes.Equal(confirmationBytes, expect) {
		return fmt.Errorf("wrong confirmation: %x", confirmationBytes)
	}
	return nil
}

func LoadConfig(configFile, adapterName string) ([]byte, string, string, error) {
	return loadConfigWithLogger(caddy.Log(), configFile, adapterName)
}

func isCaddyfile(configFile, adapterName string) (bool, error) {
	if adapterName == "caddyfile" {
		return true, nil
	}

	baseConfig := strings.ToLower(filepath.Base(configFile))
	baseConfigExt := filepath.Ext(baseConfig)
	startsOrEndsInCaddyfile := strings.HasPrefix(baseConfig, "caddyfile") || strings.HasSuffix(baseConfig, ".caddyfile")

	if baseConfigExt == ".json" {
		return false, nil
	}
	if adapterName == "" && startsOrEndsInCaddyfile {
		return true, nil
	}
	return false, nil
}
func loadConfigWithLogger(logger *zap.Logger, configFile, adapterName string) ([]byte, string, string, error) {

	if logger == nil {
		logger = zap.NewNop()
	}
	if adapterName != "" && configFile == "" {
		return nil, "", "", fmt.Errorf("cannot adapt config without config file (use --config)")
	}
	var config []byte
	var cfgAdapter caddyconfig.Adapter
	var err error
	if configFile != "" {
		if configFile == "-" {
			config, err = io.ReadAll(os.Stdin)
			if err != nil {
				return nil, "", "", fmt.Errorf("reading config from stdin: %v", err)
			}
			logger.Info("using config from stdin")
		} else {
			config, err = os.ReadFile(configFile)
			if err != nil {
				return nil, "", "", fmt.Errorf("reading config from file: %v", err)
			}
			logger.Info("using config from file", zap.String("file", configFile))
		}
	} else if adapterName == "" {
		cfgAdapter = caddyconfig.GetAdapter("caddyfile")
		if cfgAdapter != nil {
			config, err = os.ReadFile("Caddyfile")
			if errors.Is(err, fs.ErrNotExist) {
				cfgAdapter = nil
			} else if err != nil {
				return nil, "", "", fmt.Errorf("reading default Caddyfile: %v", err)
			} else {
				configFile = "Caddyfile"
				logger.Info("using adjacent Caddyfile")
			}
		}
	}

	if yes, err := isCaddyfile(configFile, adapterName); yes {
		adapterName = "caddyfile"
	} else if err != nil {
		return nil, "", "", err
	}

	// load config adapter
	if adapterName != "" {
		cfgAdapter = caddyconfig.GetAdapter(adapterName)
		if cfgAdapter == nil {
			return nil, "", "", fmt.Errorf("unrecognized config adapter: %s", adapterName)
		}
	}

	if cfgAdapter != nil {
		adaptedConfig, warnings, err := cfgAdapter.Adapt(config, map[string]any{
			"filename": configFile,
		})
		if err != nil {
			return nil, "", "", fmt.Errorf("adapting config using %s: %v", adapterName, err)
		}
		logger.Info("adapted config to JSON", zap.String("adapter", adapterName))
		for _, warn := range warnings {
			msg := warn.Message
			if warn.Directive != "" {
				msg = fmt.Sprintf("%s: %s", warn.Directive, warn.Message)
			}
			logger.Warn(msg,
				zap.String("adapter", adapterName),
				zap.String("file", warn.File),
				zap.Int("line", warn.Line))
		}
		config = adaptedConfig
	} else if len(config) != 0 {
		err = json.Unmarshal(config, new(any))
		if err != nil {
			if jsonErr, ok := err.(*json.SyntaxError); ok {
				return nil, "", "", fmt.Errorf("config is not valid JSON: %w, at offset %d; did you mean to use a config adapter (the --adapter flag)?", err, jsonErr.Offset)
			}
			return nil, "", "", fmt.Errorf("config is not valid JSON: %w; did you mean to use a config adapter (the --adapter flag)?", err)
		}
	}
	return config, configFile, adapterName, nil
}
func watchConfigFile(filename, adapterName string) {
	defer func() {
		if err := recover(); err != nil {
			log.Printf("[PANIC] watching config file: %v\n%s", err, debug.Stack())
		}
	}()
	logger := func() *zap.Logger {
		return caddy.Log().
			Named("watcher").
			With(zap.String("config_file", filename))
	}
	lastCfg, _, _, err := loadConfigWithLogger(nil, filename, adapterName)
	if err != nil {
		logger().Error("unable to load latest config", zap.Error(err))
		return
	}

	logger().Info("watching config file for changes")
	for range time.Tick(1 * time.Second) {
		newCfg, _, _, err := loadConfigWithLogger(nil, filename, adapterName)
		if err != nil {
			logger().Error("unable to load latest config", zap.Error(err))
			return
		}
		if bytes.Equal(lastCfg, newCfg) {
			continue
		}
		logger().Info("config file changed; reloading")
		lastCfg = newCfg
		err = caddy.Load(lastCfg, false)
		if err != nil {
			logger().Error("applying latest config", zap.Error(err))
			continue
		}
	}
}

type Flags struct {
	*pflag.FlagSet
}
func (f Flags) String(name string) string {
	return f.FlagSet.Lookup(name).Value.String()
}

func (f Flags) Bool(name string) bool {
	val, _ := strconv.ParseBool(f.String(name))
	return val
}
func (f Flags) Int(name string) int {
	val, _ := strconv.ParseInt(f.String(name), 0, strconv.IntSize)
	return int(val)
}
func (f Flags) Float64(name string) float64 {
	val, _ := strconv.ParseFloat(f.String(name), 64)
	return val
}
func (f Flags) Duration(name string) time.Duration {
	val, _ := caddy.ParseDuration(f.String(name))
	return val
}

func loadEnvFromFile(envFile string) error {
	file, err := os.Open(envFile)
	if err != nil {
		return fmt.Errorf("reading environment file: %v", err)
	}
	defer file.Close()

	envMap, err := parseEnvFile(file)
	if err != nil {
		return fmt.Errorf("parsing environment file: %v", err)
	}

	for k, v := range envMap {
		_, exists := os.LookupEnv(k)
		if !exists {
			if err := os.Setenv(k, v); err != nil {
				return fmt.Errorf("setting environment variables: %v", err)
			}
		}
	}
	caddy.ConfigAutosavePath = filepath.Join(caddy.AppConfigDir(), "autosave.json")
	caddy.DefaultStorage = &certmagic.FileStorage{Path: caddy.AppDataDir()}
	return nil
}

func parseEnvFile(envInput io.Reader) (map[string]string, error) {
	envMap := make(map[string]string)
	scanner := bufio.NewScanner(envInput)
	var lineNumber int
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		lineNumber++
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		before, after, isCut := strings.Cut(line, "=")
		if !isCut {
			return nil, fmt.Errorf("can't parse line %d; line should be in KEY=VALUE format", lineNumber)
		}
		key, val := before, after
		key = strings.TrimPrefix(key, "export ")

		if key == "" {
			return nil, fmt.Errorf("missing or empty key on line %d", lineNumber)
		}
		if strings.Contains(key, " ") {
			return nil, fmt.Errorf("invalid key on line %d: contains whitespace: %s", lineNumber, key)
		}
		if strings.HasPrefix(val, " ") || strings.HasPrefix(val, "\t") {
			return nil, fmt.Errorf("invalid value on line %d: whitespace before value: '%s'", lineNumber, val)
		}
		if commentStart, _, found := strings.Cut(val, "#"); found {
			val = strings.TrimRight(commentStart, " \t")
		}
		if strings.HasPrefix(val, `"`) || strings.HasPrefix(val, "'") {
			quote := string(val[0])
			for !strings.HasSuffix(line, quote) || strings.HasSuffix(line, `\`+quote) {
				val = strings.ReplaceAll(val, `\`+quote, quote)
				if !scanner.Scan() {
					break
				}
				lineNumber++
				line = strings.ReplaceAll(scanner.Text(), `\`+quote, quote)
				val += "\n" + line
			}
			val = strings.TrimPrefix(val, quote)
			val = strings.TrimSuffix(val, quote)
		}

		envMap[key] = val
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}

	return envMap, nil
}

func printEnvironment() {
	_, version := caddy.Version()
	fmt.Printf("caddy.HomeDir=%s\n", caddy.HomeDir())
	fmt.Printf("caddy.AppDataDir=%s\n", caddy.AppDataDir())
	fmt.Printf("caddy.AppConfigDir=%s\n", caddy.AppConfigDir())
	fmt.Printf("caddy.ConfigAutosavePath=%s\n", caddy.ConfigAutosavePath)
	fmt.Printf("caddy.Version=%s\n", version)
	fmt.Printf("runtime.GOOS=%s\n", runtime.GOOS)
	fmt.Printf("runtime.GOARCH=%s\n", runtime.GOARCH)
	fmt.Printf("runtime.Compiler=%s\n", runtime.Compiler)
	fmt.Printf("runtime.NumCPU=%d\n", runtime.NumCPU())
	fmt.Printf("runtime.GOMAXPROCS=%d\n", runtime.GOMAXPROCS(0))
	fmt.Printf("runtime.Version=%s\n", runtime.Version())
	cwd, err := os.Getwd()
	if err != nil {
		cwd = fmt.Sprintf("<error: %v>", err)
	}
	fmt.Printf("os.Getwd=%s\n\n", cwd)
	for _, v := range os.Environ() {
		fmt.Println(v)
	}
}
func setResourceLimits(logger *zap.Logger) func() {
	undo, err := maxprocs.Set(maxprocs.Logger(logger.Sugar().Infof))
	if err != nil {
		logger.Warn("failed to set GOMAXPROCS", zap.Error(err))
	}
	_, _ = memlimit.SetGoMemLimitWithOpts(memlimit.WithLogger(slog.New(zapslog.NewHandler(
				logger.Core(),
				zapslog.WithName("memlimit"),
				zapslog.AddStacktraceAt(slog.Level(127)),
			)),
		),
		memlimit.WithProvider(memlimit.ApplyFallback(memlimit.FromCgroup,
				memlimit.FromSystem,
			),
		),
	)

	return undo
}
// StringSlice is a flag.Value that enables repeated use of a string flag.
type StringSlice []string

func (ss StringSlice) String() string { return "[" + strings.Join(ss, ", ") + "]" }
func (ss *StringSlice) Set(value string) error {
	*ss = append(*ss, value)
	return nil
}
// Interface guard
var _ flag.Value = (*StringSlice)(nil)

module ActionMailbox
  
  class Ingresses::Mailgun::InboundEmailsController < ActionMailbox::BaseController
    before_action :authenticate
    param_encoding :create, "body-mime", Encoding::ASCII_8BIT

    def create
      ActionMailbox::InboundEmail.create_and_extract_message_id! mail
    rescue MalformedEmailError, MalformedRecipientError => error
      logger.error error.message
      head ActionDispatch::Constants::UNPROCESSABLE_CONTENT
    end

    private
      class MalformedEmailError < StandardError
        def initialize(message = "Malformed Mailgun raw email")
          super
        end
      end

      class MalformedRecipientError < StandardError
        def initialize(message = "Malformed Mailgun recipient")
          super
        end
      end

      def mail
        params.require("body-mime").tap do |raw_email|
          raise MalformedEmailError unless raw_email.is_a?(String)

          raw_email.prepend("X-Original-To: ", recipient, "\n") if params.key?(:recipient)
        end
      end

      def recipient
        params.require(:recipient).tap do |recipient|
          raise MalformedRecipientError unless recipient.is_a?(String)
        end
      end

      def authenticate
        head :unauthorized unless authenticated?
      end

      def authenticated?
        if key.present?
          Authenticator.new(
            key:       key,
            timestamp: params.require(:timestamp),
            token:     params.require(:token),
            signature: params.require(:signature)
          ).authenticated?
        else
          raise ArgumentError, <<~MESSAGE.squish
            Missing required Mailgun Signing key. Set action_mailbox.mailgun_signing_key in your application's
            encrypted credentials or provide the MAILGUN_INGRESS_SIGNING_KEY environment variable.
          MESSAGE
        end
      end

      def key
        Rails.app.credentials.dig(:action_mailbox, :mailgun_signing_key) || ENV["MAILGUN_INGRESS_SIGNING_KEY"]
      end

      class Authenticator
        attr_reader :key, :timestamp, :token, :signature

        def initialize(key:, timestamp:, token:, signature:)
          @key, @timestamp, @token, @signature = key, timestamp.to_s, token, signature
          @parsed_timestamp = Integer(timestamp, exception: false)
        end

        def authenticated?
          signed? && recent?
        end

        private
          def signed?
            signature.is_a?(String) && ActiveSupport::SecurityUtils.secure_compare(signature, expected_signature)
          end

          # Allow for 2 minutes of drift between Mailgun time and local server time.
          def recent?
            @parsed_timestamp && Time.at(@parsed_timestamp) >= 2.minutes.ago
          end

          def expected_signature
            OpenSSL::HMAC.hexdigest OpenSSL::Digest::SHA256.new, key, "#{timestamp}#{token}"
          end
      end
  end
end

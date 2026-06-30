mod options;
mod preferences;
mod rendering_context;
mod servo;
mod webview;
mod webview_delegate;

extern crate servo as servo_api;

use options::ServoOptions;
use preferences::ServoPreferences;

#[derive(Default)]
pub struct ServoBuilder {
    options: Option<Box<ServoOptions>>,
    event_loop_waker: ServoEventLoopWaker,
    preferences: Option<Box<ServoPreferences>>,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ServoEventLoopWaker {
    pub wake_callback: extern "C" fn(),
}

pub extern "C" fn default_event_loop_waker_wake_callback() {}

impl Default for ServoEventLoopWaker {
    fn default() -> Self {
        Self {
            wake_callback: default_event_loop_waker_wake_callback,
        }
    }
}

impl servo_api::EventLoopWaker for ServoEventLoopWaker {
    fn clone_box(&self) -> Box<dyn servo_api::EventLoopWaker> {
        Box::new(*self)
    }

    fn wake(&self) {
        (self.wake_callback)()
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn servo_builder_create() -> *mut ServoBuilder {
    Box::into_raw(Box::new(ServoBuilder::default()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_builder_set_preferences(builder: *mut ServoBuilder,
    preferences: *mut ServoPreferences,) 
  {
    assert!(!builder.is_null(), "builder pointer must not be null");
    assert!(
        !preferences.is_null(),
        "preferences pointer must not be null"
    );
    unsafe {
        (*builder).preferences = Some(Box::from_raw(preferences));
    }
}


#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_builder_set_options(
    builder: *mut ServoBuilder,
    options: *mut ServoOptions,)
  {
    assert!(!builder.is_null(), "builder pointer must not be null");
    assert!(!options.is_null(), "options pointer must not be null");
    unsafe {
        (*builder).options = Some(Box::from_raw(options));
    }
}


#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_builder_set_event_loop_waker(builder: *mut ServoBuilder, event_loop_waker: ServoEventLoopWaker,) 
{
    assert!(!builder.is_null(), "builder pointer must not be null");

    unsafe {
        (*builder).event_loop_waker = event_loop_waker;
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_builder_build(builder: *mut ServoBuilder) -> *mut servo_api::Servo {
    assert!(!builder.is_null(), "builder pointer must not be null");
    let mut rust_builder = servo_api::ServoBuilder::default();

    let builder = unsafe { &mut *builder };

    if let Some(options) = builder.options.take() {
        rust_builder = rust_builder.opts(*options);
    }

    if let Some(preferences) = builder.preferences.take() {
        rust_builder = rust_builder.preferences(*preferences);
    }

    rust_builder = rust_builder.event_loop_waker(Box::new(builder.event_loop_waker));
    let servo = rust_builder.build();
    Box::into_raw(Box::new(servo))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_builder_free(builder: *mut ServoBuilder) 
  {
    assert!(!builder.is_null(), "builder pointer must not be null");

    unsafe {
        let _ = Box::from_raw(builder);
    }
}

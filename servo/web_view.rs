use std::ffi::{CStr, c_void};
use std::os::raw::c_char;
use std::rc::Rc;
use servo_api::{Servo, WebView, WebViewBuilder};
use crate::rendering_context::RenderingContext;
use crate::webview_delegate::ServoWebViewDelegate;

pub struct ServoWebViewBuilder {
    servo: Servo,
    rendering_context: Rc<dyn servo_api::RenderingContext>,
    url: Option<url::Url>,
    delegate: Option<ServoWebViewDelegate>,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_builder_create(servo: *mut Servo,
    context: *mut RenderingContext,
) -> *mut ServoWebViewBuilder {
    assert!(!servo.is_null(), "servo pointer must not be null");
    assert!(!context.is_null(), "context pointer must not be null");
    let servo = unsafe { &*servo };
    let boxed_c_context = unsafe { Box::from_raw(context) };
    Box::into_raw(Box::new(ServoWebViewBuilder {
        servo: servo.clone(),
        rendering_context: boxed_c_context.inner,
        url: None,
        delegate: None,
    }))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_builder_set_url(
    builder: *mut ServoWebViewBuilder,
    url: *const c_char,
) -> i32 {
    assert!(!builder.is_null(), "builder pointer must not be null");
    assert!(!url.is_null(), "url pointer must not be null");
    
    let builder = unsafe { 
        &mut *builder 
    };
    
    let url_str = unsafe { 
        CStr::from_ptr(url) 
    }.to_str().unwrap();
    
    match url::Url::parse(url_str) {
        Ok(parsed) => {
            builder.url = Some(parsed);
            0
        },
        Err(_) => -1,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_builder_set_delegate(
    builder: *mut ServoWebViewBuilder,
    delegate: ServoWebViewDelegate,
) {
    assert!(!builder.is_null(), "builder pointer must not be null");
    let builder = unsafe { &mut *builder };
    builder.delegate = Some(delegate);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_builder_build(
    builder: *mut ServoWebViewBuilder,
) -> *mut WebView {
    assert!(!builder.is_null(), "builder pointer must not be null");
    let builder = unsafe { Box::from_raw(builder) };
    let mut webview_builder = WebViewBuilder::new(&builder.servo, builder.rendering_context);

    if let Some(url) = builder.url {
        webview_builder = webview_builder.url(url);
    }
    if let Some(delegate) = builder.delegate {
        webview_builder = webview_builder.delegate(std::rc::Rc::new(delegate));
    }
    Box::into_raw(Box::new(webview_builder.build()))
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_builder_free(builder: *mut ServoWebViewBuilder) {
    assert!(!builder.is_null(), "builder pointer must not be null");
    unsafe {
        let _ = Box::from_raw(builder);
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_paint(webview: *mut WebView) {
    assert!(!webview.is_null(), "webview pointer must not be null");
    
    let webview = unsafe { 
        &*webview 
    };
    webview.paint();
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_load(webview: *mut WebView, url: *const c_char) -> i32 {
    assert!(!webview.is_null(), "webview pointer must not be null");
    assert!(!url.is_null(), "url pointer must not be null");
    let webview = unsafe { &*webview };
    let url_str = unsafe { CStr::from_ptr(url) }.to_str().unwrap();
    match url::Url::parse(url_str) {
        Ok(parsed) => {
            webview.load(parsed);
            0
        },
        Err(_) => -1,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_take_screenshot(
    webview: *mut WebView,
    callback: Option<
        unsafe extern "C" fn(
            data: *const u8,
            width: u32,
            height: u32,
            error: i32,
            user_data: *mut c_void,
        ),
    >,
    user_data: *mut c_void,
) {
    assert!(!webview.is_null(), "webview pointer must not be null");
    let webview = unsafe { &*webview };
    let Some(callback) = callback else {
        return;
    };

    webview.take_screenshot(None, move |result| {
        match result {
            Ok(image) => {
                let (width, height) = image.dimensions();
                let data = image.into_raw();
                unsafe {
                    callback(data.as_ptr(), width, height, 0, user_data);
                }
            },
            Err(error) => {
                let error_code = error as _;
                unsafe {
                    callback(std::ptr::null(), 0, 0, error_code, user_data);
                }
            },
        }
    });
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_webview_free(webview: *mut WebView) {
    assert!(!webview.is_null(), "webview pointer must not be null");
    unsafe {
        let _ = Box::from_raw(webview);
    }
}

use servo_api::Servo;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_free(servo: *mut Servo) {
    assert!(!servo.is_null(), "servo pointer must not be null");
    unsafe {
        let _: Box<Servo> = Box::from_raw(servo);
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_spin_event_loop(servo: *mut Servo) {
    assert!(!servo.is_null(), "servo pointer must not be null");
    let servo = unsafe { &*servo };

    servo.spin_event_loop();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn servo_setup_logging(servo: *mut Servo) {
    assert!(!servo.is_null(), "servo pointer must not be null");
    let servo = unsafe { &*servo };

    servo.setup_logging();
}

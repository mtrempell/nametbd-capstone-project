#include <avr/io.h>
#include <util/delay.h>
#include <Arduino.h>
#include <stdlib.h>
#include "joints.h"
#include "rosnode.h"
#include "util.h"
#include "pid.h"

// TODO: these should be moved to joints.h
#define MOTOR_MIN 300 // FIXME
#define MOTOR_MAX 500
#define ANALOG_READ_MIN 0
#define ANALOG_READ_MAX 1023

#define ERROR_LEEWAY 10

#define MAX_PWM_VAL 255


static void arduino_init(void)
{
    init();
    #if defined(USBCON)
        USB.attach();
    #endif
}

void read_pots(int16_t *current_positions, const struct joint_info *joints,
               size_t num_joints)
{
    for (size_t i = 0; i < num_joints; ++i) {
        current_positions[i] = analogRead(joints[i].pot);
        // TODO: these may vary for every joint
        //current_positions[i] = map(current_positions[i], ANALOG_READ_MIN,
        //                           ANALOG_READ_MAX, MOTOR_MIN, MOTOR_MAX);
    }
}

// limit the requested positon to the maximum position that the joint
// can safely reach.
void limit_requested_positions(int16_t *requested_positions,
                               const struct joint_info *joints,
                               size_t num_joints)
{
    for (size_t i = 0; i < num_joints; ++i) {
        if (requested_positions[i] > joints[i].max_pos) {
            requested_positions[i] = joints[i].max_pos;
            node_log_info("limiting max position to %d for joint '%s'",
                          joints[i].max_pos, joints[i].name);
        } else if (requested_positions[i] < joints[i].min_pos) {
            node_log_info("limiting min position to %d for joint '%s'",
                          joints[i].min_pos, joints[i].name);
            requested_positions[i] = joints[i].min_pos;
        }
    }
}

void run_pid(int16_t *requested_positions, const int16_t *current_positions,
             pid_state_t *pid_states, struct joint_info *joints,
             size_t num_joints)
{
    limit_requested_positions(requested_positions, joints, num_joints);

    int16_t position_err;
    int motor_speed;
    for (size_t i = 0; i < num_joints; ++i) {

        node_log_info("requested = %d, current = %d", requested_positions[i], current_positions[i]);
        position_err = abs(requested_positions[i] - current_positions[i]);
        node_log_info("err = %d (joint %s)", position_err, joints[i].name);
        if (current_positions[i] > requested_positions[i]) {
            joint_set_pwm_reverse(&joints[i]);
        } else {
            joint_set_pwm_forward(&joints[i]); 
        }

        if (position_err > ERROR_LEEWAY) {
            motor_speed = update_pid(&pid_states[i], position_err,
                                     current_positions[i]);
            if (motor_speed > MAX_PWM_VAL) motor_speed = MAX_PWM_VAL;
            if (motor_speed <= 0) motor_speed = 0;
            node_log_info("motor speed = %d (joint %s, pin %d)", motor_speed, joints[i].name, joints[i].active_pwm_pin);
            analogWrite(joints[i].active_pwm_pin, motor_speed);
        }

    }
}

// FIXME
void map_potval_to_fullscale(int16_t *positions, struct joint_info *joints)
{
    int16_t old; 
    for (size_t i = 0; i < NUM_OF_JOINTS; ++i) {
        old = positions[i];
        positions[i] = map(positions[i], joints[i].min_pos + 10, joints[i].max_pos + 10,
                           ANALOG_READ_MIN, ANALOG_READ_MAX);
    }
}

int main(void)
{
    arduino_init();
    node_init();
    node_wait_for_connection();

    struct joint_info joints[NUM_OF_JOINTS];
    get_joint_info(joints);

    pid_state_t pid_states[NUM_OF_JOINTS];
    for (size_t i = 0; i < NUM_OF_JOINTS; ++i) {
        pid_init(&pid_states[i], joints[i].kp, joints[i].ki, joints[i].kd, 0, 3000);
    }

    int16_t current_positions[NUM_OF_JOINTS];
    int16_t *requested_positions;
    while (1) {
        read_pots(current_positions, joints, NUM_OF_JOINTS);
        //map_potval_to_fullscale(current_positions, joints);
        node_publish_data(current_positions);

        if (!received_initial_positions()) continue;
        requested_positions = node_get_requested_positions();
        run_pid(requested_positions, current_positions, pid_states, joints,
                NUM_OF_JOINTS);
    }
}

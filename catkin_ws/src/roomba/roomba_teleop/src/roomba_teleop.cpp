#include "roomba_teleop/roomba_teleop.h"

RoombaTeleop::RoombaTeleop()
    :local_nh("~"),
     auto_flag(false), move_flag(true), dock_flag(false),
     current_linear_x_vel(0.0), current_linear_y_vel(0.0), current_angular_vel(0.0)
{
    //subscriber
    cmd_sub = nh.subscribe("/planner/cmd_vel", 1, &RoombaTeleop::CommandCallback, this);
    joy_sub = nh.subscribe("/joy", 1, &RoombaTeleop::JoyCallback, this);

    //publisher
    vel_pub = nh.advertise<geometry_msgs::Twist>("/cmd_vel", 1, true);
    dock_pub = nh.advertise<std_msgs::Empty>("/dock", 1, true);
    undock_pub = nh.advertise<std_msgs::Empty>("/undock", 1, true);

    // param
    local_nh.param("HZ", HZ, {20});
    local_nh.param("MAX_SPEED", MAX_SPEED, {0.5});
    local_nh.param("MAX_YAWRATE", MAX_YAWRATE, {1.0});
    local_nh.param("VEL_RATIO", VEL_RATIO, {0.5});
    local_nh.param("accel_factor", accel_factor, {0.3});
    local_nh.param("min_change_threshold", min_change_threshold, {0.001});
}

void RoombaTeleop::CommandCallback(const geometry_msgs::TwistConstPtr& msg)
{
    cmd_vel = *msg;
}

// Smooth acceleration function using a sigmoid-like approach
double RoombaTeleop::smoothAcceleration(double current, double target, double factor) {
    // Calculate the difference between current and target
    double diff = target - current;
    
    // If the difference is very small, just return the target to avoid tiny oscillations
    if (std::abs(diff) < min_change_threshold) {
        return target;
    }
    
    // Apply a smooth acceleration factor
    return current + diff * factor;
}

void RoombaTeleop::JoyCallback(const sensor_msgs::JoyConstPtr& msg)
{
    sensor_msgs::Joy joy = *msg;
    
    // Set flags based on button presses
    if(joy.buttons[2]){
        auto_flag = false;
    }else if(joy.buttons[3]){
        auto_flag = true;
    }
    
    if(joy.buttons[0]){
        move_flag = false;
    }else if(joy.buttons[1]){
        move_flag = true;
    }
    
    if(joy.buttons[6]){
        dock_flag = false;
    }else if(joy.buttons[7]){
        dock_flag = true;
    }

    // Process joystick input for movement
    // Map Up/Down arrow keys (axis 1) to linear X movement (forward/backward)
    double linear_x_target = joy.axes[1] * MAX_SPEED;
    
    // Map Left/Right arrow keys (axis 0) to linear Y movement (left/right)
    double linear_y_target = joy.axes[0] * MAX_SPEED;
    
    // Map WASD keys (axes 6) to angular movement (a for left, d for right)
    double angular_z_target = 0.0;
    if (joy.axes[6] == -1.0) {  // A key - rotate left
        angular_z_target = VEL_RATIO * MAX_YAWRATE;
    } else if (joy.axes[6] == 1.0) {  // D key - rotate right
        angular_z_target = -VEL_RATIO * MAX_YAWRATE;
    }
    
    // Set target velocities for the joy_vel message
    joy_vel.linear.x = linear_x_target;
    joy_vel.linear.y = linear_y_target;
    joy_vel.angular.z = angular_z_target;
}

void RoombaTeleop::process()
{
    ros::Rate loop_rate(HZ);
    std_msgs::Empty empty_msgs;
    bool pre_dock_flag = dock_flag;
    
    while(ros::ok()){
        geometry_msgs::Twist vel;
        ROS_INFO("==== roomba teleop ====");
        
        if(dock_flag){
            if(!pre_dock_flag) dock_pub.publish(empty_msgs);
            ROS_INFO("docking");
        }else{
            if(pre_dock_flag) undock_pub.publish(empty_msgs);
            ROS_INFO_STREAM((move_flag ? "move" : "stop") << " : (" << (auto_flag ? "auto" : "manual") << ")");
            
            if(move_flag){
                if(auto_flag) {
                    // Use autonomous command velocity
                    double target_linear_x = cmd_vel.linear.x;
                    double target_linear_y = cmd_vel.linear.y;
                    double target_angular_z = cmd_vel.angular.z;
                    
                    // Apply smooth acceleration to command velocity
                    current_linear_x_vel = smoothAcceleration(current_linear_x_vel, target_linear_x, accel_factor);
                    current_linear_y_vel = smoothAcceleration(current_linear_y_vel, target_linear_y, accel_factor);
                    current_angular_vel = smoothAcceleration(current_angular_vel, target_angular_z, accel_factor);
                    
                } else {
                    // Use joystick control velocity
                    double target_linear_x = joy_vel.linear.x;
                    double target_linear_y = joy_vel.linear.y;
                    double target_angular_z = joy_vel.angular.z;
                    
                    // Apply smooth acceleration to joystick commands
                    current_linear_x_vel = smoothAcceleration(current_linear_x_vel, target_linear_x, accel_factor);
                    current_linear_y_vel = smoothAcceleration(current_linear_y_vel, target_linear_y, accel_factor);
                    current_angular_vel = smoothAcceleration(current_angular_vel, target_angular_z, accel_factor);
                }
                
                // Apply limits
                vel.linear.x = std::min(std::max(current_linear_x_vel, -MAX_SPEED), MAX_SPEED);
                vel.linear.y = std::min(std::max(current_linear_y_vel, -MAX_SPEED), MAX_SPEED);
                vel.angular.z = std::min(std::max(current_angular_vel, -MAX_YAWRATE), MAX_YAWRATE);
                ROS_INFO_STREAM("Current velocity - linear X: " << vel.linear.x 
                                << ", linear Y: " << vel.linear.y 
                                << ", angular: " << vel.angular.z);
            }else{
                // If movement is disabled, gradually slow down rather than immediate stop
                current_linear_x_vel = smoothAcceleration(current_linear_x_vel, 0.0, accel_factor * 2);
                current_linear_y_vel = smoothAcceleration(current_linear_y_vel, 0.0, accel_factor * 2);
                current_angular_vel = smoothAcceleration(current_angular_vel, 0.0, accel_factor * 2);
                
                vel.linear.x = current_linear_x_vel;
                vel.linear.y = current_linear_y_vel;
                vel.angular.z = current_angular_vel;
                
                // If velocities are very small, just set to zero
                if (std::abs(vel.linear.x) < min_change_threshold) vel.linear.x = 0.0;
                if (std::abs(vel.linear.y) < min_change_threshold) vel.linear.y = 0.0;
                if (std::abs(vel.angular.z) < min_change_threshold) vel.angular.z = 0.0;
            }
            
            // Publish velocity command
            vel_pub.publish(vel);
        }
        
        pre_dock_flag = dock_flag;
        loop_rate.sleep();
        ros::spinOnce();
    }
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "roomba_teleop");

    RoombaTeleop roomba_teleop;
    roomba_teleop.process();

    return 0;
}
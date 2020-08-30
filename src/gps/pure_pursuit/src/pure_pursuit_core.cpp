#include <vector>
#include <pure_pursuit_core.h>
#include <fstream>
#include <cstdlib>

#include <tf/transform_broadcaster.h>

namespace waypoint_follower
{
// Constructor
PurePursuitNode::PurePursuitNode()
  : private_nh_("~")
  , pp_()
  , LOOP_RATE_(30)
  , is_waypoint_set_(false)
  , is_pose_set_(false)
  , const_lookahead_distance_(4.0)
  , const_velocity_(3.0)
  , final_constant(1.5)
{
  initForROS();
}

// Destructor
PurePursuitNode::~PurePursuitNode() {}

void PurePursuitNode::initForROS()
{
  // ros parameter settings
  private_nh_.param("const_lookahead_distance", const_lookahead_distance_, 4.0);
  private_nh_.param("const_velocity", const_velocity_, 3.0);
  private_nh_.param("final_constant", final_constant, 1.5);
  nh_.param("vehicle_info/wheel_base", wheel_base_, 1.04);

  ROS_HOME = ros::package::getPath("pure_pursuit");

  // setup subscriber
  pose_sub = nh_.subscribe("current_pose", 1,
    &PurePursuitNode::callbackFromCurrentPose, this);

  // setup publisher
  drive_msg_pub = nh_.advertise<race::drive_values>("control_value", 1);
  steering_vis_pub = nh_.advertise<geometry_msgs::PoseStamped>("steering_vis", 1);

  // for visualization
  target_point_pub = nh_.advertise<geometry_msgs::PointStamped>("target_point", 1);
}

void PurePursuitNode::run(char** argv) {
  ROS_INFO_STREAM("pure pursuit start");

  // temp
  const_lookahead_distance_ = atof(argv[2]);
  const_velocity_ = atof(argv[3]);
  final_constant = atof(argv[4]);
  //////////////////////////

  ros::Rate loop_rate(LOOP_RATE_);
  while (ros::ok()) {
    ros::spinOnce();

    // TODO
    // 1. setPath에서 global_path, parking_path, avoidance_path 각각에 path 넣어주기
    // 2. 모드와 장애물 탐지 여부에 따라 pp_.setWaypoints를 통해 pp_가 추종하는 waypoint 바꿔주기
    //    이때, pp_.next_waypoint_number_ 를 -1로 바꿔줌으로 현재 위치를 기반으로 target point를 재설정한다.
    // 3. Mode에 따라 vel, ld, final_constant 값을 설정해준다.
    // 4. 차의 현재 위치와 목표 정지점 사이의 거리를 계산해, 그 거리가 일정 수준안에 들어왔을 때 신호등의 신호에 맞춰 행동한다.
    // 5. 주차의 경우 마지막 path에 도달하면, 분기점에 도달할때까지 어떻게든 후진하고, global_path로 전환한다. (ld = 2, vel = 3 적당했음)

    // 6. Mode = 0 : 주차구간
    //    Mode = 1 : 신호없는 직진 구간
    //    Mode = 2 : 신호없는 진진 + 우회전 구간
    //    Mode = 3 : 신호없는 자회전 구간
    //    Mode = 4 : 신호있는 구간 (직진)
    //    Mode = 5 : 신호있는 구간 (좌회전)
    //    Mode = 6 : 동적 장애물 구간
    //    Mode = 7 : 정적 장애물 구간
    //   좀 간략화 해야할듯

    if (!is_waypoint_set_) {
      setPath(argv);
      pp_.setWaypoints(global_path);
    }

    if (!is_pose_set_) {
      loop_rate.sleep();
      continue;
    }

    // interval OR Mode 에 따른 상수값 바꿔주기
    // if (pp_.next_waypoint_number_ >= 300) {
    //   const_velocity_ = 6;
    //   const_lookahead_distance_ = 5;
    //   final_constant = 1.5;
    // }
    // 이건 모드 버전
    // if (pp_.Mode == 1) {
    //   const_velocity_ = 6;
    //   const_lookahead_distance_ = 5;
    //   final_constant = 2.0;
    // }
    /////////////////////////////////////////////

    // if (Mode == 0) 주차
    //  if (!is_parking_waypoint_set && bool 타겟 포인트에 도달?(타겟 포인트 인덱스))
    //    const_velocity_ = 6;
    //    const_lookahead_distance_ = 5;
    //    final_constant = 1.5;
    //    pp_.setWaypoints(parking_path)
    //  if (! bool 타겟 포인트에 도달?(타겟 포인트 인덱스))
    //    pp_.setLookaheadDistance(computeLookaheadDistance());
    //    double kappa = 0;
    //    bool can_get_curvature = pp_.canGetCurvature(&kappa);
    //    publishDriveMsg(can_get_curvature, kappa);
    //  else
    //    여기에서 계속 퍼블리싱 때려 (후진후 빠져나갈수 있는 코드)
    //    if (bool 타겟 포인트(후진 로직 시작할때 위치점)에 도달?(타겟 포인트 인덱스))
    //      break

    // if (Mode == 4) // 신호등
    //   if (다른 신호 && bool 타겟 포인트에 도달?(타겟 포인트 인덱스))
    //     정지
    //   else <- 신호가 빨간 불이라도 타겟 포인트에 못 도달 했다면 OR 타겟포인트에 도달했지만 맞는 신호라면
    //     pp_.setLookaheadDistance(computeLookaheadDistance());
    //     double kappa = 0
    //     bool can_get_curvature = pp_.canGetCurvature(&kappa);
    //     publishDriveMsg(can_get_curvature, kappa);

    // if (Mode == 동적장애물)
    //   if (장애물 감지)
    //     정지
    //   else
    //     GO

    // if (Mode == 정적장애물)
    //   if (장애물 감지 && (bool)obs_detect == ON)
    //     if (count == 0)
    //       const_velocity_ = 6;
    //       const_lookahead_distance_ = 5;
    //       final_constant = 1.5;
    //       pp_.setWaypoints(avoidance_path)
    //       count++
    //       장애물 감지 잠시 off
    //     else if (count == 1)
    //       const_velocity_ = 6;
    //       const_lookahead_distance_ = 5;
    //       final_constant = 1.5;
    //       pp_.setWaypoints(global_path)
    //       count++
    //       장애물 감지 off
    //   else <- 장애물 감지 x
    //     const_velocity_ = 6;
    //     const_lookahead_distance_ = 5;
    //     final_constant = 1.5;
    //     GO

    pp_.setLookaheadDistance(computeLookaheadDistance());

    double kappa = 0;
    bool can_get_curvature = pp_.canGetCurvature(&kappa);

    publishDriveMsg(can_get_curvature, kappa);

    // target point visualization
    publishTargetPointVisualizationMsg();

    is_pose_set_ = false;
    loop_rate.sleep();
  }
}

void PurePursuitNode::publishDriveMsg(const bool& can_get_curvature, const double& kappa) const {
  race::drive_values drive_msg;
  drive_msg.throttle = can_get_curvature ? const_velocity_ : 0;

  double steering_radian = convertCurvatureToSteeringAngle(wheel_base_, kappa);
  drive_msg.steering = can_get_curvature ? (steering_radian * 180.0 / M_PI) * -1 * final_constant: 0;

  // std::cout << "steering : " << drive_msg.steering << "\tkappa : " << kappa <<std::endl;
  drive_msg_pub.publish(drive_msg);

  // for steering visualization
  publishSteeringVisualizationMsg(steering_radian);
}

double PurePursuitNode::computeLookaheadDistance() const {
  if (true) {
    return const_lookahead_distance_;
  }
}

void PurePursuitNode::callbackFromCurrentPose(const geometry_msgs::PoseStampedConstPtr& msg) {
  pp_.setCurrentPose(msg);
  is_pose_set_ = true;
}

void PurePursuitNode::setPath(char** argv) {
  std::ifstream infile(ROS_HOME + "/paths/" + argv[1]);
  std::cout << ROS_HOME + "/paths/" + argv[1] << std::endl;
  geometry_msgs::Point p;

  double x, y;
  while(infile >> x >> y) {
    p.x = x;
    p.y = y;
    global_path.push_back(p);
  }

  is_waypoint_set_ = true;
}

void PurePursuitNode::publishTargetPointVisualizationMsg () {
  geometry_msgs::PointStamped target_point_msg;
  target_point_msg.header.frame_id = "/base_link";
  target_point_msg.header.stamp = ros::Time::now();
  target_point_msg.point = pp_.getPoseOfNextTarget();
  target_point_pub.publish(target_point_msg);
}

void PurePursuitNode::publishSteeringVisualizationMsg (const double& steering_radian) const {
  double yaw = atan2(2.0 * (pp_.current_pose_.orientation.w * pp_.current_pose_.orientation.z + pp_.current_pose_.orientation.x * pp_.current_pose_.orientation.y), 1.0 - 2.0 * (pp_.current_pose_.orientation.y * pp_.current_pose_.orientation.y + pp_.current_pose_.orientation.z * pp_.current_pose_.orientation.z));

  double steering_vis = yaw + steering_radian;
  geometry_msgs::Quaternion _quat = tf::createQuaternionMsgFromYaw(steering_vis);
  geometry_msgs::PoseStamped pose;
  pose.header.stamp = ros::Time::now();
  pose.header.frame_id = "/base_link";
  pose.pose.position = pp_.current_pose_.position;
  pose.pose.orientation = _quat;
  steering_vis_pub.publish(pose);
}

double convertCurvatureToSteeringAngle(const double& wheel_base, const double& kappa) {
  return atan(wheel_base * kappa);
}

}  // namespace waypoint_follower

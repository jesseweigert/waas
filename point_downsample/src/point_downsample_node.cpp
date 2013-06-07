#include <ros/ros.h>
#include <ros/console.h>

#include <std_msgs/Int32.h>
#include <std_msgs/Float64.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/PointCloud2.h>
#include <geometry_msgs/QuaternionStamped.h>
#include <visualization_msgs/Marker.h>

#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>

// PCL specific includes
#include <pcl-1.6/pcl/ros/conversions.h>
#include <pcl-1.6/pcl/point_cloud.h>
#include <pcl-1.6/pcl/point_types.h>
#include <pcl_ros/transforms.h>
#include <pcl-1.6/pcl/filters/voxel_grid.h>
#include <pcl-1.6/pcl/filters/plane_clipper3D.h>
#include <pcl-1.6/pcl/features/normal_3d.h>
#include <pcl-1.6/pcl/filters/extract_indices.h>
#include <pcl-1.6/pcl/filters/passthrough.h>

#include <pcl-1.6/pcl/sample_consensus/model_types.h>
#include <pcl-1.6/pcl/sample_consensus/method_types.h>
#include <pcl-1.6/pcl/segmentation/sac_segmentation.h>
#include <pcl-1.6/pcl/segmentation/organized_multi_plane_segmentation.h>
#include <pcl-1.6/pcl/filters/statistical_outlier_removal.h>


#include <pcl-1.6/pcl/features/normal_3d.h>
#include <pcl-1.6/pcl/kdtree/kdtree.h>
#include <pcl-1.6/pcl/segmentation/extract_clusters.h>


ros::Publisher _pointsPub;
ros::Publisher _groundImuPub;
ros::Subscriber _kinectImuSub;
ros::Subscriber _pointCloudSub;

ros::NodeHandlePtr _nhPtr;
tf::TransformBroadcaster* _tfBroadcaster = NULL;
tf::TransformListener* _tfListener = NULL;

geometry_msgs::Point _kinectPosition;

//Function Prototypes
void imuCallback(const sensor_msgs::Imu::ConstPtr& imuMsg);
void pointCloudCallback (const sensor_msgs::PointCloud2Ptr& input);

int main(int argc, char** argv){
    ros::init (argc, argv, "point_downsample");
    _nhPtr = ros::NodeHandlePtr(new ros::NodeHandle());
    _tfListener = new tf::TransformListener();
    tf::TransformBroadcaster _broadcaster;
    _tfBroadcaster = &_broadcaster;


    _kinectImuSub = _nhPtr->subscribe("/imu", 10, imuCallback);
    _pointCloudSub = _nhPtr->subscribe ("/camera/depth/points", 1, pointCloudCallback);

    _pointsPub = _nhPtr->advertise<sensor_msgs::PointCloud2> ("/point_downsample/points", 1);
    _groundImuPub = _nhPtr->advertise<sensor_msgs::Imu> ("/point_downsample/ground_imu", 1);

    ros::spin();
}


void imuCallback(const sensor_msgs::Imu::ConstPtr& imuMsg) {
    tf::Quaternion orientation;
    tf::quaternionMsgToTF(imuMsg->orientation, orientation);

    tf::Transform transform;
    transform.setOrigin(tf::Vector3(_kinectPosition.x, _kinectPosition.y, _kinectPosition.z));
    transform.setRotation(orientation);

    _tfBroadcaster->sendTransform( tf::StampedTransform(transform, imuMsg->header.stamp, "base_link", "camera_link") );
}


void pointCloudCallback (const sensor_msgs::PointCloud2Ptr& input) {
    if(input->data.size() <= 0){
        std::cout << "Input cloud size " << input->data.size() << std::endl;
        return;
    }

    sensor_msgs::PointCloud2::Ptr cloud (new sensor_msgs::PointCloud2);
    sensor_msgs::PointCloud2 downSampledInput;


    //Downsample input point cloud
    pcl::VoxelGrid<sensor_msgs::PointCloud2> downsample;
    downsample.setInputCloud(input);
    downsample.setLeafSize(0.20f, 0.20f, 0.20f);
    downsample.filter(downSampledInput);

    //Transform into base_link
    try{
        pcl_ros::transformPointCloud(string("/base_link"), downSampledInput, *cloud, *_tfListener);
    }
    catch(tf::TransformException ex){
        ROS_ERROR("TFException %s",ex.what());
        return;
    }

    _pointsPub.publish(cloud);
}

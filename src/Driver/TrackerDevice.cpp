#include "TrackerDevice.hpp"
#include <PoseMath.hpp>

using namespace MocapDriver;

TrackerDevice::TrackerDevice(std::string serial, std::string role):
    serial_(serial),
    role_(role),
    motionSource_(nullptr),
    _pose_timestamp(0),
    rotation_origin(),
    segmentIndex_(-1)
{
    this->last_pose_ = MakeDefaultPose();
    this->isSetup = false;
}

std::string TrackerDevice::GetSerial()
{
    return this->serial_;
}

void TrackerDevice::reinit(int msaved, double mtime, double msmooth, UniverseOrigin origin)
{
    if (msaved < 5)     //prevent having too few values to calculate linear interpolation, and prevent crash on 0
        msaved = 5;

    if (msmooth < 0)
        msmooth = 0;
    else if (msmooth > 0.99)
        msmooth = 0.99;

    max_saved = msaved;
    std::vector<std::vector<double>> temp(msaved, std::vector<double>(8,-1));
    prev_positions = temp;
    max_time = mtime;
    smoothing = msmooth;

    translation_origin[0] = origin.translation[0];
    translation_origin[1] = origin.translation[1];
    translation_origin[2] = origin.translation[2];

    double euler_rot_origin[3] = { 0.0, origin.yaw, 0.0 };
    double quat_rot_origin[4] = {};
    eulerToQuaternion(euler_rot_origin, quat_rot_origin);
    rotation_origin.w = quat_rot_origin[3];
    rotation_origin.x = quat_rot_origin[0];
    rotation_origin.y = quat_rot_origin[1];
    rotation_origin.z = quat_rot_origin[2];

    //Log("Settings changed! " + std::to_string(msaved) + " " + std::to_string(mtime));
}

void MocapDriver::TrackerDevice::SetMotionSource(IMocapStreamSource* motionSource)
{
    motionSource_ = motionSource;
}

void MocapDriver::TrackerDevice::SetSegmentIndex(int segmentIndex)
{
    segmentIndex_ = segmentIndex;
}

int MocapDriver::TrackerDevice::GetSegmentIndex()
{
    return segmentIndex_;
}

IMocapStreamSource* MocapDriver::TrackerDevice::GetMotionSource()
{
    return motionSource_;
}

void TrackerDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
        return;

    // Check if this device was asked to be identified
    auto events = GetDriver()->GetOpenVREvents();
    for (auto event : events) {
        // Note here, event.trackedDeviceIndex does not necissarily equal this->device_index_, not sure why, but the component handle will match so we can just use that instead
        //if (event.trackedDeviceIndex == this->device_index_) {
        if (event.eventType == vr::EVREventType::VREvent_Input_HapticVibration) {
            if (event.data.hapticVibration.componentHandle == this->haptic_component_) {
                this->did_vibrate_ = true;
            }
        }
        //}
    }

    // Check if we need to keep vibrating
    if (this->did_vibrate_) {
        this->vibrate_anim_state_ += (GetDriver()->GetLastFrameTime().count() / 1000.f);
        if (this->vibrate_anim_state_ > 1.0f) {
            this->did_vibrate_ = false;
            this->vibrate_anim_state_ = 0.0f;
        }
    }

    // Setup pose for this frame
    auto tracker_pose = this->last_pose_;

    // Update time delta (for working out velocity)
    std::chrono::milliseconds time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    double time_since_epoch_seconds = time_since_epoch.count() / 1000.0;
    double pose_time_delta_seconds = (time_since_epoch - _pose_timestamp).count() / 1000.0;

    // Update pose timestamp
    _pose_timestamp = time_since_epoch;

    // Get motion source
    auto source = GetMotionSource();
    if (source) {
        auto pose = source->GetNextPose();
        auto segmentIndex = GetSegmentIndex();
        if (segmentIndex < 0 || !pose.segments.size()) {
            return;
        }

        tracker_pose.vecPosition[0] = pose.segments[segmentIndex].translation[0];
        tracker_pose.vecPosition[1] = pose.segments[segmentIndex].translation[1];
        tracker_pose.vecPosition[2] = pose.segments[segmentIndex].translation[2];

        tracker_pose.qRotation.w = pose.segments[segmentIndex].rotation_quat[0];
        tracker_pose.qRotation.x = pose.segments[segmentIndex].rotation_quat[1];
        tracker_pose.qRotation.y = pose.segments[segmentIndex].rotation_quat[2];
        tracker_pose.qRotation.z = pose.segments[segmentIndex].rotation_quat[3];


        // Set world origin from universe standing position
        tracker_pose.vecWorldFromDriverTranslation[0] = translation_origin[0];
        tracker_pose.vecWorldFromDriverTranslation[1] = translation_origin[1];
        tracker_pose.vecWorldFromDriverTranslation[2] = translation_origin[2];

        // Set world rotation from universe yaw 
        tracker_pose.qWorldFromDriverRotation = rotation_origin;
    }


    // Copy the previous position data
    double previous_position[3] = { 0 };
    std::copy(std::begin(tracker_pose.vecPosition), std::end(tracker_pose.vecPosition), std::begin(previous_position));

    //double next_pose[7];
    //if (get_next_pose(0, next_pose) != 0)
        //return;

    //normalizeQuat(next_pose);

    //bool pose_nan = false;
    //for (int i = 0; i < 7; i++)
    //{
    //    if (isnan(next_pose[i]))
    //        pose_nan = true;
    //}

    /*if (smoothing == 0 || pose_nan)
    {
        pose.vecPosition[0] = next_pose[0];
        pose.vecPosition[1] = next_pose[1];
        pose.vecPosition[2] = next_pose[2];

        pose.qRotation.w = next_pose[3];
        pose.qRotation.x = next_pose[4];
        pose.qRotation.y = next_pose[5];
        pose.qRotation.z = next_pose[6];
    }*/
   /* else
    {
        pose.vecPosition[0] = next_pose[0] * (1 - smoothing) + pose.vecPosition[0] * smoothing;
        pose.vecPosition[1] = next_pose[1] * (1 - smoothing) + pose.vecPosition[1] * smoothing;
        pose.vecPosition[2] = next_pose[2] * (1 - smoothing) + pose.vecPosition[2] * smoothing;

        pose.qRotation.w = next_pose[3] * (1 - smoothing) + pose.qRotation.w * smoothing;
        pose.qRotation.x = next_pose[4] * (1 - smoothing) + pose.qRotation.x * smoothing;
        pose.qRotation.y = next_pose[5] * (1 - smoothing) + pose.qRotation.y * smoothing;
        pose.qRotation.z = next_pose[6] * (1 - smoothing) + pose.qRotation.z * smoothing;
    }*/
    //normalize
    //double mag = sqrt(pose.qRotation.w * pose.qRotation.w +
    //    pose.qRotation.x * pose.qRotation.x +
    //    pose.qRotation.y * pose.qRotation.y +
    //    pose.qRotation.z * pose.qRotation.z);

    //pose.qRotation.w /= mag;
    //pose.qRotation.x /= mag;
    //pose.qRotation.y /= mag;
    //pose.qRotation.z /= mag;

    
    if (pose_time_delta_seconds > 0)            //unless we get two pose updates at the same time, update velocity so steamvr can do some interpolation
    {
        tracker_pose.vecVelocity[0] = 0.8 * tracker_pose.vecVelocity[0] + 0.2 * (tracker_pose.vecPosition[0] - previous_position[0]) / pose_time_delta_seconds;
        tracker_pose.vecVelocity[1] = 0.8 * tracker_pose.vecVelocity[1] + 0.2 * (tracker_pose.vecPosition[1] - previous_position[1]) / pose_time_delta_seconds;
        tracker_pose.vecVelocity[2] = 0.8 * tracker_pose.vecVelocity[2] + 0.2 * (tracker_pose.vecPosition[2] - previous_position[2]) / pose_time_delta_seconds;
    }

    //pose.poseTimeOffset = this->wantedTimeOffset;

    tracker_pose.poseTimeOffset = 0;
    //tracker_pose.vecVelocity[0] = (tracker_pose.vecPosition[0] - previous_position[0]) / pose_time_delta_seconds;
    //tracker_pose.vecVelocity[1] = (tracker_pose.vecPosition[1] - previous_position[1]) / pose_time_delta_seconds;
    //tracker_pose.vecVelocity[2] = (tracker_pose.vecPosition[2] - previous_position[2]) / pose_time_delta_seconds;

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, tracker_pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = tracker_pose;
}

void TrackerDevice::Log(std::string message)
{
    std::string message_endl = message + "\n";
    vr::VRDriverLog()->Log(message_endl.c_str());
}

/*
int TrackerDevice::get_next_pose(double time_offset, double pred[])
{
    int statuscode = 0;

    std::chrono::milliseconds time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    double time_since_epoch_seconds = time_since_epoch.count() / 1000.0;

    double req_time = time_since_epoch_seconds - time_offset;

    double new_time = last_update - req_time;

    if (new_time < -0.2)      //limit prediction to max 0.2 second into the future to prevent your feet from being yeeted into oblivion
    {
        new_time = -0.2;
        statuscode = 1;
    }

    int curr_saved = 0;
    //double pred[7] = {0};

    double avg_time = 0;
    double avg_time2 = 0;
    for (int i = 0; i < max_saved; i++)
    {
        if (prev_positions[i][0] < 0)
            break;
        curr_saved++;
        avg_time += prev_positions[i][0];
        avg_time2 += (prev_positions[i][0] * prev_positions[i][0]);
    }

    //Log("saved values: " + std::to_string(curr_saved));

    //printf("curr saved %d\n", curr_saved);
    if (curr_saved < 4)
    {
        if (curr_saved > 0)
        {
            for (int i = 1; i < 8; i++)
            {
                pred[i - 1] = prev_positions[0][i];
            }
            return statuscode;
        }
        //printf("Too few values");
        statuscode = -1;
        return statuscode;
        //return 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;
    }
    avg_time /= curr_saved;
    avg_time2 /= curr_saved;

    //printf("avg time %f\n", avg_time);

    double st = 0;
    for (int j = 0; j < curr_saved; j++)
    {
        st += ((prev_positions[j][0] - avg_time) * (prev_positions[j][0] - avg_time));
    }
    st = sqrt(st * (1.0 / curr_saved));


    for (int i = 1; i < 8; i++)
    {
        double avg_val = 0;
        double avg_val2 = 0;
        double avg_tval = 0;
        for (int ii = 0; ii < curr_saved; ii++)
        {
            avg_val += prev_positions[ii][i];
            avg_tval += (prev_positions[ii][0] * prev_positions[ii][i]);
            avg_val2 += (prev_positions[ii][i] * prev_positions[ii][i]);
        }
        avg_val /= curr_saved;
        avg_tval /= curr_saved;
        avg_val2 /= curr_saved;

        //printf("--avg: %f\n", avg_val);

        double sv = 0;
        for (int j = 0; j < curr_saved; j++)
        {
            sv += ((prev_positions[j][i] - avg_val) * (prev_positions[j][i] - avg_val));
        }
        sv = sqrt(sv * (1.0 / curr_saved));

        //printf("----sv: %f\n", sv);

        double rxy = (avg_tval - (avg_val * avg_time)) / sqrt((avg_time2 - (avg_time * avg_time)) * (avg_val2 - (avg_val * avg_val)));
        double b = rxy * (sv / st);
        double a = avg_val - (b * avg_time);

        //printf("a: %f, b: %f\n", a, b);

        double y = a + b * new_time;
        //Log("aha: " + std::to_string(y) + std::to_string(avg_val));
        if (abs(avg_val2 - (avg_val * avg_val)) < 0.00000001)               //bloody floating point rounding errors
            y = avg_val;

        pred[i - 1] = y;
        //printf("<<<< %f --> %f\n",y, pred[i-1]);


    }
    //printf("::: %f\n", pred[0]);
    return statuscode;
    //return pred[0], pred[1], pred[2], pred[3], pred[4], pred[5], pred[6];
}

void TrackerDevice::save_current_pose(double a, double b, double c, double w, double x, double y, double z, double time_offset)
{
    if (max_time == 0)
    {
        std::chrono::milliseconds time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        double time_since_epoch_seconds = time_since_epoch.count() / 1000.0;
        double curr_time = time_since_epoch_seconds;
        this->last_update = curr_time;
        prev_positions[0][0] = time_offset;
        prev_positions[0][1] = a;
        prev_positions[0][2] = b;
        prev_positions[0][3] = c;
        prev_positions[0][4] = w;
        prev_positions[0][5] = x;
        prev_positions[0][6] = y;
        prev_positions[0][7] = z;

        return;
    }

    double next_pose[7];
    int pose_valid = get_next_pose(time_offset, next_pose);

    double dot = x * next_pose[4] + y * next_pose[5] + z * next_pose[6] + w * next_pose[3];

    if (dot < 0)
    {
        x = -x;
        y = -y;
        z = -z;
        w = -w;
    } 

    //update times
    std::chrono::milliseconds time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    double time_since_epoch_seconds = time_since_epoch.count() / 1000.0;

    //Log("time since epoch: " + std::to_string(time_since_epoch_seconds));
    
    //lock_t curr_time = clock();
    //clock_t capture_time = curr_time - (timeOffset*1000);
    double curr_time = time_since_epoch_seconds;
    double time_since_update = curr_time - this->last_update;
    this->last_update = curr_time;

    for (int i = 0; i < max_saved; i++)
    {
        if (prev_positions[i][0] >= 0)
            prev_positions[i][0] += time_since_update;
        if (prev_positions[i][0] > max_time)
            prev_positions[i][0] = -1;
    }

    double time = time_offset;
    // double offset = (rand() % 100) / 10000.;
    // time += offset;
    // printf("%f %f\n", time, offset);

    //Log("Time: " + std::to_string(time));

    double dist = sqrt(pow(next_pose[0] - a, 2) + pow(next_pose[1] - b, 2) + pow(next_pose[2] - c, 2));
    if (pose_valid == 0 && dist > 0.5)
    {
        Log("Dropped a pose! its error was " + std::to_string(dist));
        Log("Height vs predicted height:" + std::to_string(b) + " " + std::to_string(next_pose[1]));
        return;
    }

    dist = sqrt(pow(a, 2) + pow(b, 2) + pow(c, 2));
    if (dist > 10)
    {
        Log("Dropped a pose! Was outside of playspace: " + std::to_string(dist));
        return;
    }

    //if (time > max_time)
    //    return;

    if (prev_positions[max_saved - 1][0] < time && prev_positions[max_saved - 1][0] >= 0)
        return;

    int i = 0;
    while (prev_positions[i][0] < time&& prev_positions[i][0] >= 0)
        i++;

    for (int j = max_saved - 1; j > i; j--)
    {
        if (prev_positions[j - 1][0] >= 0)
        {
            for (int k = 0; k < 8; k++)
            {
                prev_positions[j][k] = prev_positions[j - 1][k];
            }
        }
        else
        {
            prev_positions[j][0] = -1;
        }
    }
    prev_positions[i][0] = time;
    prev_positions[i][1] = a;
    prev_positions[i][2] = b;
    prev_positions[i][3] = c;
    prev_positions[i][4] = w;
    prev_positions[i][5] = x;
    prev_positions[i][6] = y;
    prev_positions[i][7] = z;
                                                     //for debugging
    //Log("------------------------------------------------");
    //for (int i = 0; i < max_saved; i++)
    //{
    //    Log("Time: " + std::to_string(prev_positions[i][0]));
    //    Log("Position x: " + std::to_string(prev_positions[i][1]));
    //}
    //
    return;
}
*/

DeviceType TrackerDevice::GetDeviceType()
{
    return DeviceType::TRACKER;
}

vr::TrackedDeviceIndex_t TrackerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError TrackerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating tracker " + this->serial_);

    // Get the properties handle
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 3);
    
    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "Mocap_tracker");

    // Opt out of hand selection
    GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_OptOut);

    // Set up a render model path
    std::string rendermodel = "{Mocap}/rendermodels/" + motionSource_->GetRenderModelPath(GetSegmentIndex());
    Log("Mocap segment rendermodel path: " + rendermodel);
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, rendermodel.c_str());

    // Set controller profile
    //GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{Mocap}/input/example_tracker_bindings.json");

    // Set the icon
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{Mocap}/icons/tracker_ready.png");

    if (this->serial_.find("Mocap") == std::string::npos)
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{Mocap}/icons/tracker_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{Mocap}/icons/tracker_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{Mocap}/icons/tracker_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{Mocap}/icons/tracker_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{Mocap}/icons/tracker_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{Mocap}/icons/tracker_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{Mocap}/icons/tracker_not_ready.png");
    }
    else
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{Mocap}/icons/MVN_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{Mocap}/icons/MVN_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{Mocap}/icons/MVN_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{Mocap}/icons/MVN_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{Mocap}/icons/MVN_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{Mocap}/icons/MVN_not_ready.png");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{Mocap}/icons/MVN_not_ready.png");
    }
    /*
    char id = this->serial_.at(12);
    std::string role = "";
    switch (id)
    {
    case '0':
        role = "vive_tracker_waist"; break;
    case '0':
        role = "vive_tracker_left_foot"; break;
    case '1':
        role = "vive_tracker_right_foot"; break;
    }
    */

    //set role, role hint and everything else to ensure trackers are detected as trackers and not controllers

    std::string rolehint = "vive_tracker";
    if (role_ == "TrackerRole_LeftFoot")
        rolehint = "vive_tracker_left_foot";
    else if (role_ == "TrackerRole_RightFoot")
        rolehint = "vive_tracker_right_foot";
    else if (role_ == "TrackerRole_Waist")
        rolehint = "vive_tracker_waist";

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ControllerType_String, role_.c_str());

    vr::VRProperties()->SetInt32Property(props, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_GenericTracker);
    vr::VRProperties()->SetInt32Property(props, vr::Prop_ControllerHandSelectionPriority_Int32, -1);

    std::string l_registeredDevice("/devices/MVN/");
    l_registeredDevice.append(serial_);

    Log("Setting role " + role_ + " to " + l_registeredDevice);

    if(role_ != "vive_tracker")
        vr::VRSettings()->SetString(vr::k_pch_Trackers_Section, l_registeredDevice.c_str(), role_.c_str());

    return vr::EVRInitError::VRInitError_None;
}

void TrackerDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void TrackerDevice::EnterStandby()
{
}

void* TrackerDevice::GetComponent(const char* pchComponentNameAndVersion)
{
    return nullptr;
}

void TrackerDevice::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

vr::DriverPose_t TrackerDevice::GetPose()
{
    return last_pose_;
}
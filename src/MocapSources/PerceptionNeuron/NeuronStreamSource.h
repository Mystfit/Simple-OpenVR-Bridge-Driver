#pragma once

#include <IMocapStreamSource.hpp>
#include <MocapApi.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <string>
#include <mutex>

#include "BVH.h"
#include "NeuronSegments.h"


#define NEURON_STREAM "AxisNeuron"

class NeuronStreamSource : public IMocapStreamSource {

public:
	// ctor & dtor
	//NeuronStreamSource();
	//~NeuronStreamSource();

	// Implementing IMocapStreamSource
	virtual void Init(MocapDriver::IVRDriver* owning_driver) override;
	virtual bool Connect() override;
	virtual void Update() override;
	virtual void PopulateTrackers() override;
	virtual MocapDriver::IVRDriver* GetDriver() override;
	virtual PoseSample GetNextPose() override;
	virtual void QueuePose(const PoseSample& pose);
	virtual std::string GetRenderModelPath(int segmentIndex);
	

private:
	MocapApi::MCPApplicationHandle_t _application;
	void UpdateAvatarTransform(MocapApi::MCPAvatarHandle_t avatar);
	void UpdateJointsTransform(MocapApi::MCPJointHandle_t joint, MocapApi::MCPAvatarHandle_t avatar, PoseSample& outPose);

	std::string GetSegmentRole(NeuronSegment segment);


	// See below for more information
	enum {
		BVHBoneCount = 59,
		CalcBoneCount = 21,
	};

	MocapDriver::IVRDriver* owning_driver_;
	std::unordered_map<NeuronSegment, std::shared_ptr<MocapDriver::IVRDevice>> trackers_;

	BVH bvhLoader;
	bool m_firstWrite;
	double m_initialFrame, m_currentFrame;
	int m_perJoint;
	int m_dataCount;
	std::map<int, std::string> m_bones;

	std::mutex pose_update_mtx;
	PoseSample completed_pose_;
};

/*

SKELETON DATA SEQUENCE IN ARRAY :

----------------------
Hips				0
----------------------
RightUpLeg			1
----------------------
RightLeg			2
----------------------
RightFoot			3
----------------------
LeftUpLeg			4
----------------------
LeftLeg				5
----------------------
LeftFoot			6
----------------------
Spine				7
----------------------
Spine1				8
----------------------
Spine2				9
----------------------
Spine3				10
----------------------
Neck				11
----------------------
Head				12
----------------------
Right Shoulder		13
----------------------
RightArm			14
----------------------
RightForeArm		15
----------------------
RightHand			16
----------------------
RightHandThumb1		17
----------------------
RightHandThumb2		18
----------------------
RightHandThumb3		19
----------------------
RightInHandIndex	20
----------------------
RightHandIndex1		21
----------------------
RightHandIndex2		22
----------------------
RightHandIndex3		23
----------------------
RightInHandMiddle	24
----------------------
RightHandMiddle1	25
----------------------
RightHandMiddle2	26
----------------------
RightHandMiddle3	27
----------------------
RightInHandRing		28
----------------------
RightHandRing1		29
----------------------
RightHandRing2		30
----------------------
RightHandRing3		31
----------------------
RightInHandPinky	32
----------------------
RightHandPinky1		33
----------------------
RightHandPinky2		34
----------------------
RightHandPinky3		35
----------------------
Left Shoulder		36
----------------------
LeftArm				37
----------------------
LeftForeArm			38
----------------------
LeftHand			39
----------------------
LeftHandThumb1		40
----------------------
LeftHandThumb2		41
----------------------
LeftHandThumb3		42
----------------------
LeftInHandIndex		43
----------------------
LeftHandIndex1		44
----------------------
LeftHandIndex2		45
----------------------
LeftHandIndex3		46
----------------------
LeftInHandMiddle	47
----------------------
LeftHandMiddle1		48
----------------------
LeftHandMiddle2		49
----------------------
LeftHandMiddle3		50
----------------------
LeftInHandRing		51
----------------------
LeftHandRing1		52
----------------------
LeftHandRing2		53
----------------------
LeftHandRing3		54
----------------------
LeftInHandPinky		55
----------------------
LeftHandPinky1		56
----------------------
LeftHandPinky2		57
----------------------
LeftHandPinky3		58
----------------------

*/

/*
BONE SEQUENCE TABLE (CALC) :

0. Hips
1. RightUpLeg
2. RightLeg
3. RightFoot
4. LeftUpLeg
5. LeftLeg
6. LeftFoot
7. RightShoulder
8. RightArm
9. RightForeArm
10. RightHand
11. LeftShoulder
12. LeftArm
13. LeftForeArm
14. LeftHand
15. Head
16. Neck
17. Spine3
18. Spine2
19. Spine1
20. Spine

*/

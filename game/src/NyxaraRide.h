#pragma once

// Nyxara on a horse: her pose, made by code from her skeleton (2026-11-27; NyxaraHorse.h has the horses, the owner's
// words and her ways of riding).
//
// Her hips on its seat; her body over them, more upright than the horse's back and leaned into its going (forward at a
// gallop, back as it stops); her neck and head kept level (the kit turns her head after this: LookAbout); her feet in
// its stirrups, her knees forward and out round its barrel, her heels down and her toes a little out; her hands before
// her on the reins. Her getting on and down is her own pose (her clip's) blended into this and out of it, her hips
// along a leap's arc between where she stands and the seat.
//
// All it is given is in the world (the horse's seat, stirrups and reins as its pose has them this frame) and is brought
// into her model's frame by her picture's matrix: her model's own ways and units do not matter here. Her joints are
// found by name as her flight's poser finds them (NyxaraFlight.h), and her limbs are set as it sets them: each bone
// aimed along its way, its fold toward the way it bends (a two-bone reach for each arm and leg).

#include "DoorwaysAnimationPose.h"
#include "DoorwaysSkeleton.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace Doorways::NyxaraRide
{
    // The horse this frame, in the world
    struct Saddle
    {
        DirectX::XMFLOAT3 Seat{};                                         // the middle of its seat, where her hips sit
        DirectX::XMFLOAT3 Forward{0, 0, 1}, Up{0, 1, 0}, Right{1, 0, 0};  // its body's ways (Right: the horse's right)
        DirectX::XMFLOAT3 Stirrup[2]{};                                   // the treads: her left foot's, her right's
        DirectX::XMFLOAT3 Rein[2]{};                                      // where her left and right hands hold the reins
        float Yaw = 0;                                                    // her facing (the horse's)
        float Lean = 0;                                                   // rad her body leans forward (+) over her hips
        float Rise = 0;                                                   // m her hips are over the seat (out of it at a gallop)
    };
    // Her getting on or down: how far she is on (0 her own pose, 1 on the horse), and her leap
    struct Blend
    {
        float Weight = 1;   // 0 .. 1
        float Arc = 0;      // m her hips are lifted over the straight way between where she stands and the seat
        float Tuck = 0;     // 0 .. 1 her knees drawn up in the leap
    };

    class Poser
    {
      public:
        bool Bind(const DoorwaysSkeleton& skeleton)
        {
            using namespace DirectX;
            mBound = false;
            const auto& joints = skeleton.Joints;
            const auto find = [&](const char* name) {
                const std::string want(name);
                for (size_t i = 0; i < joints.size(); ++i)
                {
                    const std::string& n = joints[i].Name;
                    if (n.size() >= want.size() && n.compare(n.size() - want.size(), want.size(), want) == 0 &&
                        (n.size() == want.size() || n[n.size() - want.size() - 1] == ':' || n[n.size() - want.size() - 1] == '_'))
                        return static_cast<int>(i);
                }
                return -1;
            };
            mHips = find("Hips"); mSpine[0] = find("Spine"); mSpine[1] = find("Spine1"); mSpine[2] = find("Spine2");
            mNeck = find("Neck"); mHead = find("Head");
            const char* arms[2][4] = {{"LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand"}, {"RightShoulder", "RightArm", "RightForeArm", "RightHand"}};
            const char* legs[2][4] = {{"LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase"}, {"RightUpLeg", "RightLeg", "RightFoot", "RightToeBase"}};
            const char* dress[2][2] = {{"DressLeft", "DressLeftKnee"}, {"DressRight", "DressRightKnee"}};
            bool all = mHips >= 0 && mSpine[0] >= 0 && mSpine[1] >= 0 && mSpine[2] >= 0 && mNeck >= 0 && mHead >= 0;
            for (int s = 0; s < 2; ++s)
            {
                for (int k = 0; k < 4; ++k) { mArm[s][k] = find(arms[s][k]); mLeg[s][k] = find(legs[s][k]); all = all && mArm[s][k] >= 0 && mLeg[s][k] >= 0; }
                for (int k = 0; k < 2; ++k) mDress[s][k] = find(dress[s][k]);
            }
            if (!all) return false;
            mFingers.clear();   // (her fingers: all below her hands, by name)
            for (size_t i = 0; i < joints.size(); ++i)
                for (const std::string_view hand : {std::string_view("LeftHand"), std::string_view("RightHand")})
                    if (const size_t k = joints[i].Name.find(hand); k != std::string::npos && k + hand.size() < joints[i].Name.size())
                    {
                        mFingers.push_back(static_cast<int>(i));
                        break;
                    }
            const size_t n = joints.size();
            mParent.assign(n, -1); mLocal.assign(n, {}); mModel.assign(n, {}); mAt.assign(n, {});
            for (size_t i = 0; i < n; ++i)
            {
                const auto& j = joints[i];
                if (j.ParentIndex >= static_cast<int>(i)) return false;   // (parents come first)
                mParent[i] = j.ParentIndex;
                const XMVECTOR local = XMQuaternionNormalize(XMLoadFloat4(&j.LocalBindRotation));
                XMStoreFloat4(&mLocal[i], local);
                const XMVECTOR parentModel = j.ParentIndex >= 0 ? XMLoadFloat4(&mModel[static_cast<size_t>(j.ParentIndex)]) : XMQuaternionIdentity();
                XMStoreFloat4(&mModel[i], XMQuaternionNormalize(XMQuaternionMultiply(local, parentModel)));
                const XMVECTOR at = j.ParentIndex >= 0 ? XMLoadFloat3(&mAt[static_cast<size_t>(j.ParentIndex)]) + XMVector3Rotate(XMLoadFloat3(&j.LocalBindTranslation), parentModel)
                                                      : XMLoadFloat3(&j.LocalBindTranslation);
                XMStoreFloat3(&mAt[i], at);
            }
            const XMVECTOR along = XMVectorSet(0, 1, 0, 0);
            for (int s = 0; s < 2; ++s)
            {
                {   // her elbow: the way her forearm leans off her upper arm at bind is the way it folds; its angle there
                    const XMVECTOR upper = XMQuaternionInverse(Model(mArm[s][1]));
                    const XMVECTOR fore = XMVector3Normalize(XMVector3Rotate(XMVector3Normalize(At(mArm[s][3]) - At(mArm[s][2])), upper));
                    const float c = std::clamp(XMVectorGetY(fore), -1.f, 1.f);
                    mElbowBind[s] = std::acos(c);
                    XMVECTOR fold = fore - along * c;
                    fold = XMVectorGetX(XMVector3LengthSq(fold)) < 1e-6f ? XMVectorSet(-1, 0, 0, 0) : XMVector3Normalize(fold);
                    XMStoreFloat3(&mArmFold[s], fold);
                    XMStoreFloat3(&mElbowAxis[s], XMVector3Normalize(XMVector3Cross(along, fold)));
                }
                {   // her knee folds her shin back: the thigh's own way to her back, at bind
                    const XMVECTOR back = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), XMQuaternionInverse(Model(mLeg[s][0])));
                    const XMVECTOR fold = XMVector3Normalize(back - along * XMVectorGetY(back));
                    XMStoreFloat3(&mLegFold[s], fold);
                    XMStoreFloat3(&mKneeAxis[s], XMVector3Normalize(XMVector3Cross(along, fold)));
                }
                mUpper[s] = XMVectorGetX(XMVector3Length(At(mArm[s][2]) - At(mArm[s][1])));
                mFore[s] = XMVectorGetX(XMVector3Length(At(mArm[s][3]) - At(mArm[s][2])));
                mThigh[s] = XMVectorGetX(XMVector3Length(At(mLeg[s][1]) - At(mLeg[s][0])));
                mShin[s] = XMVectorGetX(XMVector3Length(At(mLeg[s][2]) - At(mLeg[s][1])));
            }
            mBound = true;
            return true;
        }
        bool Bound() const { return mBound; }

        // pose: her pose as her clip has it (her model's), changed here. picture: her model into the world, as the engine
        // draws her this frame. grip: a pose of hers with her fists closed (her fingers are taken from it), or none.
        void Pose(const Saddle& sd, const Blend& bl, DirectX::FXMMATRIX picture, DoorwaysSkeletonPose& pose, const DoorwaysSkeletonPose* grip = nullptr) const
        {
            using namespace DirectX;
            if (!mBound || pose.LocalJoints.size() != mLocal.size()) return;
            const size_t n = mLocal.size();
            const float w = std::clamp(bl.Weight, 0.f, 1.f);
            const XMMATRIX toModel = XMMatrixInverse(nullptr, picture);
            const auto place = [&](FXMVECTOR world) { return XMVector3TransformCoord(XMVectorSetW(world, 1), toModel); };
            const auto way = [&](FXMVECTOR world) { return XMVector3Normalize(XMVector3TransformNormal(XMVectorSetW(world, 0), toModel)); };
            const auto load = [](const XMFLOAT3& v) { return XMLoadFloat3(&v); };

            // her clip's pose: each joint's turn and place in her model
            std::vector<XMFLOAT4> model(n);
            std::vector<XMFLOAT3> at(n);
            const auto evaluate = [&]
            {
                for (size_t j = 0; j < n; ++j)
                {
                    const auto& lj = pose.LocalJoints[j];
                    const int parent = mParent[j];
                    const XMVECTOR pm = parent >= 0 ? XMLoadFloat4(&model[static_cast<size_t>(parent)]) : XMQuaternionIdentity();
                    XMStoreFloat4(&model[j], XMQuaternionNormalize(XMQuaternionMultiply(XMLoadFloat4(&lj.Rotation), pm)));
                    const XMVECTOR a = parent >= 0 ? XMLoadFloat3(&at[static_cast<size_t>(parent)]) + XMVector3Rotate(XMLoadFloat3(&lj.Translation), pm)
                                                   : XMLoadFloat3(&lj.Translation);
                    XMStoreFloat3(&at[j], a);
                }
            };
            evaluate();
            const auto M = [&](int j) { return XMLoadFloat4(&model[static_cast<size_t>(j)]); };
            const auto A = [&](int j) { return XMLoadFloat3(&at[static_cast<size_t>(j)]); };
            const auto parentModel = [&](int j) { return mParent[static_cast<size_t>(j)] >= 0 ? M(mParent[static_cast<size_t>(j)]) : XMQuaternionIdentity(); };
            // a joint turned so (in her model), its parent as it is now
            const auto set = [&](int j, FXMVECTOR m)
            {
                XMStoreFloat4(&pose.LocalJoints[static_cast<size_t>(j)].Rotation, XMQuaternionNormalize(Then(m, XMQuaternionInverse(parentModel(j)))));
                XMStoreFloat4(&model[static_cast<size_t>(j)], XMQuaternionNormalize(m));
            };
            const auto mix = [&](int j, FXMVECTOR ride) { return XMQuaternionSlerp(M(j), ride, w); };   // (her clip's turn of it toward this)

            // ---- the ways, in her model: hers as she stands (the world's up, her facing), the horse's
            const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
            const XMVECTOR up0 = way(worldUp), fwd0 = way(XMVectorSet(std::sin(sd.Yaw), 0, std::cos(sd.Yaw), 0));
            const XMVECTOR U = load(sd.Up), F = load(sd.Forward), R = load(sd.Right);   // (world)
            const XMVECTOR up = way(U), fwd = way(F);
            const XMVECTOR seated = Align(fwd0, up0, fwd, up);   // her standing frame onto the horse's

            // ---- her hips: from where her clip has them (standing where she is) to the seat, along the leap's arc
            {
                const XMVECTOR stand = XMVector3TransformCoord(XMVectorSetW(A(mHips), 1), picture);   // (world)
                const XMVECTOR seat = load(sd.Seat) + U * (HipOverSeat + sd.Rise) - F * .02f;
                const float e = w * w * (3 - 2 * w);
                const XMVECTOR hipsWorld = XMVectorLerp(stand, seat, e) + worldUp * bl.Arc;
                const XMVECTOR hipsAt = place(hipsWorld);
                const int parent = mParent[static_cast<size_t>(mHips)];
                const XMVECTOR pm = parentModel(mHips);
                const XMVECTOR pa = parent >= 0 ? A(parent) : XMVectorZero();
                XMStoreFloat3(&pose.LocalJoints[static_cast<size_t>(mHips)].Translation, XMVector3Rotate(hipsAt - pa, XMQuaternionInverse(pm)));
                set(mHips, mix(mHips, Then(Model(mHips), seated)));
            }
            // ---- her back: more upright than the horse's, leaned into its going; her neck and head level
            {
                const XMVECTOR upright = XMVector3Normalize(XMVectorLerp(up, up0, .6f));
                const XMVECTOR leaned = XMVector3Normalize(upright * std::cos(sd.Lean) + fwd * std::sin(sd.Lean));
                const XMVECTOR torso = Align(fwd0, up0, fwd, leaned);
                const XMVECTOR level = XMVector3Normalize(fwd - up0 * XMVectorGetX(XMVector3Dot(fwd, up0)));
                const XMVECTOR head = Align(fwd0, up0, level, up0);
                constexpr float share[3] = {.35f, .7f, 1.f};
                for (int i = 0; i < 3; ++i) set(mSpine[i], mix(mSpine[i], Then(Model(mSpine[i]), XMQuaternionSlerp(seated, torso, share[i]))));
                set(mNeck, mix(mNeck, Then(Model(mNeck), XMQuaternionSlerp(torso, head, .5f))));
                set(mHead, mix(mHead, Then(Model(mHead), head)));
            }
            evaluate();   // (her shoulders and hips where her back now has them)
            const XMVECTOR rightM = way(R);
            // ---- her legs: her feet in the stirrups, her knees forward and out round its barrel
            for (int s = 0; s < 2; ++s)
            {
                const int* l = mLeg[s];
                const XMVECTOR out = s == 0 ? -R : R;                               // (world: her left foot on its left)
                const XMVECTOR outM = s == 0 ? -rightM : rightM;
                const XMVECTOR tread = load(sd.Stirrup[s]);
                XMVECTOR ankleWorld = tread + U * .075f - F * .05f;
                if (bl.Tuck > 0) ankleWorld = XMVectorLerp(ankleWorld, load(sd.Seat) + out * .35f + F * .25f + U * .05f, bl.Tuck);   // (knees up in the leap)
                const XMVECTOR hip = A(l[0]);
                const XMVECTOR ankle = place(ankleWorld);
                const XMVECTOR pole = way(F * .8f + out * .6f);
                XMVECTOR end;
                const XMVECTOR knee = Reach(hip, ankle, mThigh[s], mShin[s], pole, end);
                const XMVECTOR upper = XMVector3Normalize(knee - hip);
                XMVECTOR lower = end - knee;
                lower = XMVectorGetX(XMVector3LengthSq(lower)) < 1e-10f ? upper : XMVector3Normalize(lower);
                XMVECTOR bend = lower - upper * XMVectorGetX(XMVector3Dot(lower, upper));
                bend = XMVectorGetX(XMVector3LengthSq(bend)) < 1e-8f ? -fwd : XMVector3Normalize(bend);
                const XMVECTOR thigh = Aimed(upper, bend, XMLoadFloat3(&mLegFold[s]));
                const float kneeAngle = std::acos(std::clamp(XMVectorGetX(XMVector3Dot(upper, lower)), -1.f, 1.f));
                const XMVECTOR shin = Then(Then(LocalBind(l[1]), About(XMLoadFloat3(&mKneeAxis[s]), kneeAngle)), thigh);
                // her foot along the horse, its toes a little out and her heel down
                const float toeOut = .26f, heel = .2f;
                XMVECTOR toe = XMVector3Normalize(fwd * std::cos(toeOut) + outM * std::sin(toeOut));
                const XMVECTOR sole = XMVector3Normalize(up * std::cos(heel) - toe * std::sin(heel));
                toe = XMVector3Normalize(toe * std::cos(heel) + up * std::sin(heel));
                const XMVECTOR foot = Then(Model(l[2]), Align(fwd0, up0, toe, sole));
                set(l[0], mix(l[0], thigh));
                set(l[1], mix(l[1], shin));
                set(l[2], mix(l[2], foot));
                if (mDress[s][0] >= 0) pose.LocalJoints[static_cast<size_t>(mDress[s][0])].Rotation = pose.LocalJoints[static_cast<size_t>(l[0])].Rotation;
                if (mDress[s][1] >= 0) pose.LocalJoints[static_cast<size_t>(mDress[s][1])].Rotation = pose.LocalJoints[static_cast<size_t>(l[1])].Rotation;
            }
            // ---- her arms: her hands on the reins before her, her elbows back and a little out
            for (int s = 0; s < 2; ++s)
            {
                const int* a = mArm[s];
                const XMVECTOR out = s == 0 ? -R : R;
                const XMVECTOR shoulder = A(a[1]);
                const XMVECTOR hand = place(load(sd.Rein[s]));
                const XMVECTOR pole = way(-F * .6f - U * .55f + out * .45f);
                XMVECTOR end;
                const XMVECTOR elbow = Reach(shoulder, hand, mUpper[s], mFore[s], pole, end);
                const XMVECTOR upper = XMVector3Normalize(elbow - shoulder);
                XMVECTOR lower = end - elbow;
                lower = XMVectorGetX(XMVector3LengthSq(lower)) < 1e-10f ? upper : XMVector3Normalize(lower);
                XMVECTOR bend = lower - upper * XMVectorGetX(XMVector3Dot(lower, upper));
                bend = XMVectorGetX(XMVector3LengthSq(bend)) < 1e-8f ? fwd : XMVector3Normalize(bend);
                const XMVECTOR arm = Aimed(upper, bend, XMLoadFloat3(&mArmFold[s]));
                const float elbowAngle = std::acos(std::clamp(XMVectorGetX(XMVector3Dot(upper, lower)), -1.f, 1.f));
                set(a[1], mix(a[1], arm));
                const XMVECTOR foreLocal = Then(LocalBind(a[2]), About(XMLoadFloat3(&mElbowAxis[s]), elbowAngle - mElbowBind[s]));
                const XMVECTOR foreModel = Then(foreLocal, M(a[1]));
                set(a[2], mix(a[2], foreModel));
            }
            // ---- her fingers closed on the reins, as her fists are on guard (2026-11-27: her hands open, palms up, looked
            //      held out to something, not holding)
            if (grip && grip->LocalJoints.size() == n)
                for (const int j : mFingers)
                {
                    auto& fingerTurn = pose.LocalJoints[static_cast<size_t>(j)].Rotation;
                    XMStoreFloat4(&fingerTurn, XMQuaternionSlerp(XMLoadFloat4(&fingerTurn), XMLoadFloat4(&grip->LocalJoints[static_cast<size_t>(j)].Rotation), w));
                }
        }

        // how high her hips joint is over what she sits on (m)
        static constexpr float HipOverSeat = .1f;

      private:
        static DirectX::XMVECTOR Then(DirectX::FXMVECTOR first, DirectX::FXMVECTOR second) { return DirectX::XMQuaternionMultiply(first, second); }
        static DirectX::XMVECTOR About(DirectX::FXMVECTOR axis, float radians) { return DirectX::XMQuaternionRotationAxis(axis, radians); }
        // the turn that takes her local Y to y and her local X to x (made square to y)
        static DirectX::XMVECTOR Frame(DirectX::FXMVECTOR yWay, DirectX::FXMVECTOR xWay)
        {
            using namespace DirectX;
            const XMVECTOR y = XMVector3Normalize(XMVectorSetW(yWay, 0));
            XMVECTOR x = XMVectorSetW(xWay, 0);
            x = x - y * XMVectorGetX(XMVector3Dot(x, y));
            if (XMVectorGetX(XMVector3LengthSq(x)) < 1e-8f)
            {
                x = XMVector3Cross(y, XMVectorSet(0, 0, 1, 0));
                if (XMVectorGetX(XMVector3LengthSq(x)) < 1e-6f) x = XMVector3Cross(y, XMVectorSet(1, 0, 0, 0));
            }
            x = XMVector3Normalize(x);
            const XMVECTOR z = XMVector3Cross(x, y);
            const XMMATRIX m(x, y, z, XMVectorSet(0, 0, 0, 1));
            return XMQuaternionNormalize(XMQuaternionRotationMatrix(m));
        }
        // a bone turned so that its length points along way and its own side (local) along side
        static DirectX::XMVECTOR Aimed(DirectX::FXMVECTOR way, DirectX::FXMVECTOR side, DirectX::FXMVECTOR localSide)
        {
            using namespace DirectX;
            return XMQuaternionNormalize(Then(XMQuaternionInverse(Frame(XMVectorSet(0, 1, 0, 0), localSide)), Frame(way, side)));
        }
        // the turn that takes a frame (its forward and up) onto another
        static DirectX::XMVECTOR Align(DirectX::FXMVECTOR fromForward, DirectX::FXMVECTOR fromUp, DirectX::FXMVECTOR toForward, DirectX::GXMVECTOR toUp)
        {
            using namespace DirectX;
            const XMVECTOR fromSide = XMVector3Cross(fromUp, fromForward), toSide = XMVector3Cross(toUp, toForward);
            return XMQuaternionNormalize(Then(XMQuaternionInverse(Frame(fromUp, fromSide)), Frame(toUp, toSide)));
        }
        // a two-bone limb from root toward goal, bent toward pole: its middle joint; end: where its end comes (the goal,
        // or as near as the limb reaches)
        static DirectX::XMVECTOR Reach(DirectX::FXMVECTOR root, DirectX::FXMVECTOR goal, float first, float second, DirectX::FXMVECTOR pole, DirectX::XMVECTOR& end)
        {
            using namespace DirectX;
            XMVECTOR d = goal - root;
            const float len = (std::max)(1e-4f, XMVectorGetX(XMVector3Length(d)));
            d = d / len;
            const float reach = std::clamp(len, std::abs(first - second) + 1e-4f, (first + second) * .999f);
            end = root + d * reach;
            const float c = std::clamp((first * first + reach * reach - second * second) / (2 * first * reach), -1.f, 1.f);
            XMVECTOR side = pole - d * XMVectorGetX(XMVector3Dot(pole, d));
            if (XMVectorGetX(XMVector3LengthSq(side)) < 1e-8f)
            {
                side = XMVector3Cross(d, XMVectorSet(1, 0, 0, 0));
                if (XMVectorGetX(XMVector3LengthSq(side)) < 1e-8f) side = XMVector3Cross(d, XMVectorSet(0, 0, 1, 0));
            }
            side = XMVector3Normalize(side);
            return root + (d * c + side * std::sqrt((std::max)(0.f, 1 - c * c))) * first;
        }
        DirectX::XMVECTOR Model(int j) const { return DirectX::XMLoadFloat4(&mModel[static_cast<size_t>(j)]); }
        DirectX::XMVECTOR LocalBind(int j) const { return DirectX::XMLoadFloat4(&mLocal[static_cast<size_t>(j)]); }
        DirectX::XMVECTOR At(int j) const { return DirectX::XMLoadFloat3(&mAt[static_cast<size_t>(j)]); }

        bool mBound = false;
        int mHips = -1, mSpine[3]{-1, -1, -1}, mNeck = -1, mHead = -1;
        int mArm[2][4]{}, mLeg[2][4]{}, mDress[2][2]{};
        std::vector<int> mParent, mFingers;
        std::vector<DirectX::XMFLOAT4> mLocal, mModel;   // bind: local and model turns
        std::vector<DirectX::XMFLOAT3> mAt;              // bind: places (model)
        DirectX::XMFLOAT3 mArmFold[2]{}, mElbowAxis[2]{}, mLegFold[2]{}, mKneeAxis[2]{};
        float mElbowBind[2]{}, mUpper[2]{}, mFore[2]{}, mThigh[2]{}, mShin[2]{};
    };
}

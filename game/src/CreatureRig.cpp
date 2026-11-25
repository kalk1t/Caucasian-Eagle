#include "CreatureRig.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

using namespace DirectX;

namespace CaucasianEagle
{
    namespace
    {
        using R = CreatureRig;
        constexpr float Pi = 3.14159265f;

        // Angles are radians, each turned in this order: about x, then y, then z, in its parent's frame (the model
        // faces +z, its right is +x). About x: positive tips the top of a part forward (a chest bows) and swings a
        // hanging part back (an arm, a leg); negative raises a hanging arm forward (-pi/2 level before it, -2.4 up
        // over its head). About y: positive turns the part to its right (its right side back; an arm held out before
        // it swings out to its right). About z: positive swings a hanging part over to its right (+x): the right arm
        // out, the left arm in; a chest leans over to its left.
        struct Angles { float X = 0, Y = 0, Z = 0; };
        using Pose = Angles[R::JointCount];

        float Lerp(float a, float b, float t) { return a + (b - a) * t; }
        float Ease(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3 - 2 * t); }
        float Span(float t, float from, float to) { return Ease((t - from) / (to - from)); }
        float Arc(float t) { return std::sin(Pi * std::clamp(t, 0.f, 1.f)); }   // 0, up to 1 halfway, 0 again

        // The blow itself. 'strike' falls from 1 to 0: over the first quarter the part sweeps from where the windup
        // left it ('drawn') to the end of the blow ('hit'), then it returns to rest.
        float Blow(float strike, float drawn, float hit)
        {
            return strike > .75f ? Lerp(hit, drawn, (strike - .75f) / .25f) : hit * Ease(strike / .75f);
        }

        // Its left for its right: the parts swap sides and every turn about y and z goes the other way.
        void Mirror(Pose& a)
        {
            const std::pair<int, int> sides[] = {{R::ArmL, R::ArmR}, {R::ForeL, R::ForeR}, {R::LegL, R::LegR}, {R::ShinL, R::ShinR},
                                                 {R::WingL, R::WingR}, {R::WingL2, R::WingR2}};
            for (const auto& [l, r] : sides) std::swap(a[l], a[r]);
            for (auto& j : a) { j.Y = -j.Y; j.Z = -j.Z; }
        }
        void Add(Pose& to, const Pose& from)
        {
            for (int j = 0; j < R::JointCount; ++j) { to[j].X += from[j].X; to[j].Y += from[j].Y; to[j].Z += from[j].Z; }
        }

        // ---- the blows of the two-legged and robed (with the right side; Mirror gives the left). d: how far it is
        //      drawn back (0..1, held at 1 through the action), s: the blow (1 at the moment it lands, falling to 0),
        //      t: through the action (0..1)
        void Blows(R::Attack attack, float d, float s, float t, float time, Pose& a)
        {
            const auto B = [s](float drawn, float hit) { return Blow(s, drawn, hit); };
            switch (attack)
            {
            case R::Attack::Swing:   // the right arm goes up and back over the shoulder, the body turns away, then everything comes through
                a[R::Chest].X += -.30f * d + B(-.30f, .55f);  a[R::Chest].Y += .45f * d + B(.45f, -.50f);
                a[R::ArmR].X += -2.4f * d + B(-2.4f, -.70f);  a[R::ForeR].X += -.50f * d + B(-.50f, -.10f);
                a[R::ArmL].X += -.50f * d + B(-.50f, .50f);
                a[R::LegL].X += -.25f * d + B(-.25f, -.45f);  a[R::LegR].X += .20f * d + B(.20f, .40f);
                a[R::Head].X += .15f * d;
                break;
            case R::Attack::Slam:    // both arms over the head, the back arched, then down
                a[R::Chest].X += -.42f * d + B(-.42f, .70f);
                a[R::ArmL].X += -2.6f * d + B(-2.6f, -.80f);  a[R::ArmR].X += -2.6f * d + B(-2.6f, -.80f);
                a[R::ForeL].X += -.40f * d;                   a[R::ForeR].X += -.40f * d;
                a[R::LegL].X += B(0, -.30f); a[R::ShinL].X += B(0, .35f);
                a[R::LegR].X += B(0, -.30f); a[R::ShinR].X += B(0, .35f);
                a[R::Head].X += .15f * d;
                break;
            case R::Attack::Cast:    // hands drawn back and thrust forward, the cloth thrown back
                a[R::Chest].X += -.20f * d + B(-.20f, .30f);
                a[R::ArmL].X += -.70f * d + B(-.70f, -1.5f); a[R::ArmL].Z += -.35f * d;
                a[R::ArmR].X += -.70f * d + B(-.70f, -1.5f); a[R::ArmR].Z += .35f * d;
                a[R::ForeL].X += -.80f * d; a[R::ForeR].X += -.80f * d;
                a[R::Skirt1].X += B(0, .25f); a[R::Skirt2].X += B(0, .35f);
                a[R::Head].X += .15f * d;
                break;
            case R::Attack::Sweep:   // the right arm held out level and far back to its right, the body turned with it, then a flat arc across
                a[R::Chest].Y += .75f * d + B(.75f, -.85f);   a[R::Chest].X += .10f * d + B(.10f, .25f);
                a[R::Head].Y += -.50f * d + B(-.50f, .55f);    // (its eyes stay on her)
                a[R::ArmR].X += -1.45f * d + B(-1.45f, -1.40f); a[R::ArmR].Y += 1.05f * d + B(1.05f, -.60f);
                a[R::ForeR].X += -.30f * d + B(-.30f, -.10f);
                a[R::ArmL].X += -.45f * d + B(-.45f, .25f);    a[R::ArmL].Z += -.20f * d + B(-.20f, -.85f);
                a[R::LegL].X += -.18f * d; a[R::LegR].X += .14f * d; a[R::LegL].Z += -.10f * d; a[R::LegR].Z += .10f * d;
                a[R::ShinL].X += .22f * d; a[R::ShinR].X += .22f * d;
                break;
            case R::Attack::Charge:  // head and shoulders down, arms swept back, the back foot pawing; then the rush; at the end it drives through
            {
                const float paw = t > 0 ? 0.f : d * std::max(0.f, std::sin(time * 11.f));
                a[R::Chest].X += .50f * d + B(.50f, .80f);    a[R::Head].X += -.38f * d + B(-.38f, -.25f);
                a[R::ArmL].X += .65f * d + B(.65f, -1.10f);   a[R::ArmR].X += .65f * d + B(.65f, -1.10f);
                a[R::ArmL].Z += -.25f * d;                    a[R::ArmR].Z += .25f * d;
                a[R::LegL].X += -.30f * d; a[R::ShinL].X += .50f * d;
                a[R::LegR].X += .25f * d + .25f * paw; a[R::ShinR].X += .20f * d + .45f * paw;
                break;
            }
            case R::Attack::Pounce:  // a deep crouch; in the air stretched out, claws first, legs trailing; it lands raking down
            {
                const float fly = t > 0 ? Span(t, 0, .2f) : 0.f, crouch = d * (1 - fly);   // (landing, the blow takes over from the flight)
                a[R::Chest].X += .45f * crouch + .15f * fly + B(.15f, .55f); a[R::Head].X += -.40f * crouch - .30f * fly + B(-.30f, -.10f);
                a[R::LegL].X += -.60f * crouch + .35f * fly + B(.35f, -.45f); a[R::ShinL].X += 1.10f * crouch + .90f * fly + B(.90f, .80f);
                a[R::LegR].X += -.60f * crouch + .55f * fly + B(.55f, -.35f); a[R::ShinR].X += 1.10f * crouch + .70f * fly + B(.70f, .70f);
                a[R::ArmL].X += .55f * crouch - 1.95f * fly + B(-1.95f, -.55f); a[R::ArmR].X += .55f * crouch - 1.95f * fly + B(-1.95f, -.55f);
                a[R::ArmL].Z += -.30f * crouch - .20f * fly;  a[R::ArmR].Z += .30f * crouch + .20f * fly;
                a[R::ForeL].X += -.25f * fly + B(-.25f, -.05f); a[R::ForeR].X += -.25f * fly + B(-.25f, -.05f);
                break;
            }
            case R::Attack::Pound:   // both fists up over the head, the back arched, up on its toes; then down into the ground before it in a squat
                a[R::Chest].X += -.35f * d + B(-.35f, .95f);  a[R::Head].X += .20f * d + B(.20f, -.50f);
                a[R::ArmL].X += -2.70f * d + B(-2.70f, -1.30f); a[R::ArmR].X += -2.70f * d + B(-2.70f, -1.30f);
                a[R::ArmL].Z += .15f * d;                     a[R::ArmR].Z += -.15f * d;
                a[R::ForeL].X += -.40f * d + B(-.40f, -.15f); a[R::ForeR].X += -.40f * d + B(-.40f, -.15f);
                a[R::LegL].X += B(0, -.60f); a[R::ShinL].X += B(0, 1.05f);
                a[R::LegR].X += B(0, -.60f); a[R::ShinR].X += B(0, 1.05f);
                break;
            case R::Attack::Stamp:   // the right knee drawn up high, arms out for balance, leaning over the left foot; then the foot stamped down
                a[R::LegR].X += -1.15f * d + B(-1.15f, .05f); a[R::ShinR].X += 1.10f * d + B(1.10f, .05f);
                a[R::LegL].X += -.08f * d;                    a[R::ShinL].X += .20f * d + B(.20f, .40f);
                a[R::Chest].X += -.12f * d + B(-.12f, .30f);  a[R::Chest].Z += .10f * d;
                a[R::ArmL].Z += -.55f * d + B(-.55f, -.25f);  a[R::ArmR].Z += .55f * d + B(.55f, .25f);
                a[R::ArmL].X += -.30f * d;                    a[R::ArmR].X += -.30f * d;
                a[R::Head].X += .10f * d + B(.10f, .25f);
                break;
            case R::Attack::Throw:   // the right arm back over its shoulder, the other pointing at her, the body turned away; then through
                a[R::ArmR].X += -2.85f * d + B(-2.85f, -.75f); a[R::ArmR].Z += .30f * d + B(.30f, -.10f);
                a[R::ForeR].X += -.70f * d + B(-.70f, -.05f);
                a[R::Chest].Y += .60f * d + B(.60f, -.55f);   a[R::Chest].X += -.25f * d + B(-.25f, .40f);
                a[R::Head].Y += -.40f * d + B(-.40f, .35f);
                a[R::ArmL].X += -1.10f * d + B(-1.10f, .45f);
                a[R::LegL].X += -.35f * d + B(-.35f, -.40f);  a[R::LegR].X += .25f * d + B(.25f, .35f); a[R::ShinL].X += .15f * d;
                break;
            case R::Attack::Curse:   // both hands raised together over its head, then struck down to the ground before it
                a[R::ArmL].X += -2.85f * d + B(-2.85f, -.55f); a[R::ArmR].X += -2.85f * d + B(-2.85f, -.55f);
                a[R::ArmL].Z += -.15f * d;                    a[R::ArmR].Z += .15f * d;   // (raised, that is inward)
                a[R::ForeL].X += -.30f * d;                   a[R::ForeR].X += -.30f * d;
                a[R::Chest].X += -.30f * d + B(-.30f, .55f);  a[R::Head].X += -.25f * d + B(-.25f, .30f);
                a[R::LegL].X += B(0, -.30f); a[R::ShinL].X += B(0, .50f);
                a[R::LegR].X += B(0, -.30f); a[R::ShinR].X += B(0, .50f);
                a[R::Skirt1].X += B(0, -.20f); a[R::Skirt2].X += B(0, -.30f);
                break;
            case R::Attack::Shriek:  // hunched, arms crossed before it, claws up; then flung wide, the head thrown back, the cloth flaring
                a[R::Chest].X += .35f * d + B(.35f, -.40f);   a[R::Head].X += .25f * d + B(.25f, -.65f);
                a[R::ArmL].X += -.90f * d + B(-.90f, -1.20f); a[R::ArmR].X += -.90f * d + B(-.90f, -1.20f);
                a[R::ArmL].Z += .35f * d + B(.35f, -1.30f);   a[R::ArmR].Z += -.35f * d + B(-.35f, 1.30f);
                a[R::ForeL].X += -.90f * d + B(-.90f, -.10f); a[R::ForeR].X += -.90f * d + B(-.90f, -.10f);
                a[R::Skirt1].X += B(0, .30f); a[R::Skirt2].X += B(0, .45f);
                a[R::Skirt1].Z += B(0, .15f) * std::sin(time * 23.f); a[R::Skirt2].Z += B(0, .20f) * std::sin(time * 19.f);
                break;
            case R::Attack::Breath:  // it rears back, arms out; then chest and head thrust at her, the arms flung back
                a[R::Chest].X += -.40f * d + B(-.40f, .55f);  a[R::Head].X += -.35f * d + B(-.35f, -.35f);
                a[R::ArmL].Z += -.45f * d + B(-.45f, -.65f);  a[R::ArmR].Z += .45f * d + B(.45f, .65f);
                a[R::ArmL].X += -.50f * d + B(-.50f, .40f);   a[R::ArmR].X += -.50f * d + B(-.50f, .40f);
                a[R::ForeL].X += -.60f * d + B(-.60f, -.20f); a[R::ForeR].X += -.60f * d + B(-.60f, -.20f);
                a[R::LegL].X += -.25f * d + B(-.25f, -.35f);  a[R::ShinL].X += .20f * d + B(.20f, .35f);
                break;
            case R::Attack::Roar:    // crouched, arms crossed before it; then flung up and wide, the back arched, the head thrown back
                a[R::Chest].X += .45f * d + B(.45f, -.45f);   a[R::Head].X += .30f * d + B(.30f, -.55f);
                a[R::ArmL].X += -1.00f * d + B(-1.00f, -2.20f); a[R::ArmR].X += -1.00f * d + B(-1.00f, -2.20f);
                a[R::ArmL].Z += .45f * d + B(.45f, .80f);     a[R::ArmR].Z += -.45f * d + B(-.45f, -.80f);   // (raised high, that is out)
                a[R::ForeL].X += -1.00f * d + B(-1.00f, -.10f); a[R::ForeR].X += -1.00f * d + B(-1.00f, -.10f);
                a[R::LegL].X += -.30f * d + B(-.30f, 0); a[R::ShinL].X += .55f * d + B(.55f, 0);
                a[R::LegR].X += -.30f * d + B(-.30f, 0); a[R::ShinR].X += .55f * d + B(.55f, 0);
                break;
            default: break;
            }
        }

        // ---- the dodges of the two-legged and robed (to its right; Mirror gives its left). e: through the dodge (0..1)
        void Dodges(R::Dodge dodge, bool back, float e, float time, Pose& a)
        {
            const float h = Arc(e);   // 0, up to 1 halfway, 0 again
            switch (dodge)
            {
            case R::Dodge::SideHop:   // a crouched leap aside, knees tucked, banking into it, arms out
                a[R::LegL].X += -.75f * h; a[R::ShinL].X += 1.15f * h;
                a[R::LegR].X += -.65f * h; a[R::ShinR].X += 1.05f * h;
                a[R::Chest].X += .25f * h; a[R::Chest].Z += -.22f * h; a[R::Pelvis].Z += -.12f * h; a[R::Head].Z += .20f * h;
                a[R::ArmL].Z += -.60f * h; a[R::ArmR].Z += .35f * h; a[R::ArmL].X += -.30f * h; a[R::ArmR].X += -.30f * h;
                break;
            case R::Dodge::BackHop:   // a hop back, knees up before it, arms thrown forward for balance
                a[R::Chest].X += -.20f * h; a[R::Head].X += .15f * h;
                a[R::ArmL].X += -.90f * h; a[R::ArmR].X += -.90f * h; a[R::ForeL].X += -.40f * h; a[R::ForeR].X += -.40f * h;
                a[R::LegL].X += -.45f * h; a[R::ShinL].X += .80f * h; a[R::LegR].X += -.35f * h; a[R::ShinR].X += .75f * h;
                a[R::Skirt1].X += -.25f * h; a[R::Skirt2].X += -.35f * h;
                break;
            case R::Dodge::Glide:     // a fast drift: banking into it, arms and cloth streaming behind
                if (back)
                {
                    a[R::Chest].X += -.18f * h; a[R::ArmL].X += -.55f * h; a[R::ArmR].X += -.55f * h;
                    a[R::Skirt1].X += -.40f * h; a[R::Skirt2].X += -.55f * h;
                }
                else
                {
                    a[R::Chest].Z += -.25f * h; a[R::Chest].X += .10f * h; a[R::Head].Z += .12f * h;
                    a[R::ArmL].Z += -.55f * h; a[R::ArmR].Z += -.30f * h;
                    a[R::Skirt1].Z += -.35f * h; a[R::Skirt2].Z += -.50f * h;
                }
                a[R::Skirt2].Z += .05f * h * std::sin(time * 17.f);
                break;
            case R::Dodge::FlameStep: // crouched into its fire, arms crossed before it (its owner hides it in the middle), and up out of it
            {
                const float crouch = Span(e, 0, .3f) * (1 - Span(e, .7f, 1.f));
                a[R::LegL].X += -.50f * crouch; a[R::ShinL].X += .90f * crouch; a[R::LegR].X += -.50f * crouch; a[R::ShinR].X += .90f * crouch;
                a[R::Chest].X += .40f * crouch; a[R::Head].X += .30f * crouch;
                a[R::ArmL].X += -1.00f * crouch; a[R::ArmL].Z += .40f * crouch; a[R::ArmR].X += -1.00f * crouch; a[R::ArmR].Z += -.40f * crouch;
                a[R::ForeL].X += -.80f * crouch; a[R::ForeR].X += -.80f * crouch;
                break;
            }
            case R::Dodge::SideStep:  // a long lunge aside, ducking over the leg it goes onto, the other arm out
                if (back)
                {
                    a[R::LegR].X += .45f * h; a[R::ShinR].X += .30f * h; a[R::LegL].X += -.20f * h; a[R::ShinL].X += .45f * h;
                    a[R::Chest].X += .25f * h; a[R::Head].X += -.20f * h;
                    a[R::ArmL].X += -.80f * h; a[R::ForeL].X += -.90f * h; a[R::ArmR].X += -.80f * h; a[R::ForeR].X += -.90f * h;
                }
                else
                {
                    a[R::LegR].Z += .45f * h; a[R::LegR].X += -.25f * h; a[R::ShinR].X += .55f * h; a[R::LegL].Z += -.10f * h;
                    a[R::Chest].X += .35f * h; a[R::Chest].Z += -.25f * h; a[R::Head].X += -.25f * h; a[R::Head].Z += .15f * h;
                    a[R::ArmL].Z += -.45f * h; a[R::ArmR].X += -.60f * h; a[R::ForeR].X += -.70f * h;
                }
                break;
            case R::Dodge::HeavyStep: // a heavy step back (or aside), leaning away, its left forearm up before its face
                if (back) { a[R::LegR].X += .35f * h; a[R::ShinR].X += .25f * h; a[R::LegL].X += -.15f * h; a[R::ShinL].X += .30f * h; a[R::Chest].X += -.18f * h; }
                else { a[R::LegR].Z += .35f * h; a[R::ShinR].X += .30f * h; a[R::ShinL].X += .25f * h; a[R::Chest].Z += -.15f * h; a[R::Chest].X += -.10f * h; }
                a[R::ArmL].X += -1.35f * h; a[R::ForeL].X += -1.25f * h; a[R::ArmL].Z += .25f * h;
                a[R::ArmR].Z += .35f * h; a[R::Head].X += .15f * h;
                break;
            default: break;
            }
        }
    }

    bool CreatureRig::Load(const std::filesystem::path& path, std::string& error)
    {
        Kind = Type::None;
        std::ifstream file(path);
        if (!file) return true;
        for (int j = 0; j < JointCount; ++j) { Parent[j] = -1; Pivot[j] = {0, 0, 0}; }
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream in(line);
            std::string tag; in >> tag;
            if (tag == "TYPE")
            {
                std::string name; in >> name;
                Kind = name == "biped" ? Type::Biped : name == "robed" ? Type::Robed : name == "serpent" ? Type::Serpent : name == "winged" ? Type::Winged : Type::None;
            }
            else if (tag == "J")
            {
                int joint = -1, parent = -1; XMFLOAT3 pivot{};
                in >> joint >> parent >> pivot.x >> pivot.y >> pivot.z;
                if (!in || joint < 0 || joint >= JointCount || parent >= JointCount) { error = path.string() + ": bad joint line"; return false; }
                Parent[joint] = parent; Pivot[joint] = pivot;
            }
        }
        if (Kind == Type::None) { error = path.string() + ": no TYPE line"; return false; }
        // parents before children
        int count = 0; bool placed[JointCount]{};
        for (int pass = 0; pass < JointCount && count < JointCount; ++pass)
            for (int j = 0; j < JointCount; ++j)
                if (!placed[j] && (Parent[j] < 0 || placed[Parent[j]])) { placed[j] = true; Order[count++] = j; }
        if (count != JointCount) { error = path.string() + ": joints form a loop"; return false; }
        return true;
    }

    void BuildCreaturePalette(const CreatureRig& rig, const CreaturePose& pose, Doorways::DoorwaysGpuSkinningPalette& out, float* drop)
    {
        Pose a{};
        const CreatureGait& g = pose.Gait;
        const float walk = std::clamp(pose.Walk, 0.f, 1.f), s = std::sin(pose.Stride), c = std::cos(pose.Stride);
        const float breath = std::sin(pose.Time * 1.7f);
        const float drawn = Ease(pose.Windup), strike = std::clamp(pose.Strike, 0.f, 1.f), act = std::clamp(pose.Action, 0.f, 1.f);
        const float recoil = std::clamp(pose.Recoil, 0.f, 1.f), death = Ease(pose.Death);
        const float daze = Ease(std::clamp(pose.Daze, 0.f, 1.f)), kneel = std::clamp(pose.Kneel, 0.f, 1.f);
        const float down = std::clamp((std::max)({daze, kneel, death}), 0.f, 1.f);   // (dazed, kneeling, dying: none of the stance)
        const float ready = Ease(pose.Ready) * (1 - down), turn = std::clamp(pose.Turn, -1.f, 1.f) * (1 - down);
        const float evade = std::clamp(pose.Evade, 0.f, 1.f);

        if (rig.Kind == R::Type::Biped || rig.Kind == R::Type::Robed)
        {
            // ---- going: legs swing against each other, the knee of the leg coming forward bends, arms swing against
            //      the legs, hips and shoulders twist against each other. Turning on the spot it takes short steps.
            //      Its gait: hunched or upright, arms long and low or held out, knees lifted, a roll of the hips, a limp.
            const float step = (std::max)(walk, .5f * std::abs(turn));
            const float weak = 1 - .45f * g.Limp;   // (the weak left leg: a short, stiff step)
            // its legs swing the way it goes (Heading): ahead, back, or out to a side and in again (not as far: legs open
            // less than they swing, and its feet stay apart); turning on the spot it steps ahead. The knee of the leg
            // going the way it goes bends, whichever way that is.
            const float going = walk / (std::max)(step, 1e-4f);
            const float fore = 1 - going * (1 - std::cos(pose.Heading)), aside = going * std::sin(pose.Heading);
            // running (ahead only): the leg in front comes up high, the knee coming through bends more
            const float run = std::clamp(pose.Run, 0.f, 1.f) * walk * (std::max)(0.f, fore);
            a[R::LegL].X = -.55f * step * s * weak * fore - .30f * run * (std::max)(0.f, s);
            a[R::LegR].X = .55f * step * s * fore - .30f * run * (std::max)(0.f, -s);
            a[R::ShinL].X = .80f * g.Lift * step * (std::max)(0.f, c) * (1 - .55f * g.Limp) * (1 + .7f * run);
            a[R::ShinR].X = .80f * g.Lift * step * (std::max)(0.f, -c) * (1 + .7f * run);
            const float apart = .10f * step * std::abs(aside);
            a[R::LegL].Z = .30f * step * s * weak * aside - apart; a[R::LegR].Z = -.30f * step * s * aside + apart;
            // crouched: a hunter's bent knees, and its fighting stance
            const float crouch = std::clamp(g.Prowl * (.6f + .4f * walk) + .45f * ready, 0.f, 1.2f) * (1 - down);
            a[R::LegL].X -= .32f * crouch; a[R::ShinL].X += .62f * crouch;
            a[R::LegR].X -= .32f * crouch; a[R::ShinR].X += .62f * crouch;
            // the chest leans with its gait, its speed, its crouch and its stance; the head looks ahead all the same and the
            // arms still hang (they are turned back as far as the chest bows)
            const float lean = .10f * walk * fore + .20f * run;   // (it leans the way it goes: back as it backs away; into a run)
            const float bow = g.Hunch * (1 - down) + lean + .15f * crouch + .12f * ready;
            a[R::Chest].X = bow + .025f * breath;
            a[R::Head].X = -.06f * walk * fore - .02f * breath - .75f * (bow - lean);
            const float swing = .45f * g.Swing * (1 + .5f * g.Low) * walk * fore * (1 + .5f * run);   // (against the legs, whichever way)
            a[R::ArmL].X = swing * s - .85f * bow * (1 - .5f * ready); a[R::ArmR].X = -swing * s - .85f * bow * (1 - .5f * ready);
            a[R::ForeL].X = a[R::ForeR].X = (-.12f - .22f * walk) * (1 - .6f * g.Low) - .95f * run;   // (running: elbows bent)
            a[R::ArmL].Z = -g.Out - .10f * g.Low; a[R::ArmR].Z = g.Out + .10f * g.Low;
            a[R::Pelvis].Y = .10f * walk * s * fore; a[R::Pelvis].Z = (.03f + g.Sway + .05f * std::abs(aside)) * walk * c;
            a[R::Chest].Y = -.22f * walk * s * fore; a[R::Chest].Z = -.6f * g.Sway * walk * c;
            a[R::Head].Y = .12f * walk * s * fore;   a[R::Head].Z = .3f * g.Sway * walk * c;
            // a hobble: it dips onto the weak leg and nods
            a[R::Chest].Z += .10f * g.Limp * walk * (std::max)(0.f, -c); a[R::Head].X += .10f * g.Limp * walk * (std::max)(0.f, -c);
            // ---- standing: breathing moves the arms a little; in its stance its fists come up and it shifts from foot to foot
            a[R::ArmL].Z += -.04f * breath * (1 - walk); a[R::ArmR].Z += .04f * breath * (1 - walk);
            a[R::ArmL].X += -.45f * ready; a[R::ArmR].X += -.45f * ready;
            a[R::ForeL].X += -.55f * ready; a[R::ForeR].X += -.55f * ready;
            a[R::ArmL].Z += -.12f * ready; a[R::ArmR].Z += .12f * ready;
            a[R::Pelvis].Z += .035f * ready * std::sin(pose.Time * 2.3f) * (1 - walk);
            // ---- the cloth of the robed spirits trails behind and sways
            a[R::Skirt1].X = .22f * walk * fore + .05f * std::sin(pose.Time * 2.1f);
            a[R::Skirt1].Z = .04f * std::sin(pose.Time * 1.3f) - .16f * walk * aside;
            a[R::Skirt2].X = .30f * walk * fore + .08f * std::sin(pose.Time * 2.1f - 1.f);
            a[R::Skirt2].Z = .06f * std::sin(pose.Time * 1.3f - .8f) - .22f * walk * aside;

            // ---- the blow (its own side, or the other): the one wound up and the one landing, each on its own side
            //      (every part of a blow is either drawn back or struck, so the two add up to the blow when they are one)
            {
                Pose wound{}, landing{};
                Blows(pose.Attack, drawn, 0, act, pose.Time, wound);
                Blows(pose.Struck, 0, strike, 0, pose.Time, landing);
                if (pose.Mirror) Mirror(wound);
                if (pose.StruckMirror) Mirror(landing);
                Add(a, wound); Add(a, landing);
            }
            // ---- the dodge (to its right; to its left mirrored; back)
            if (pose.Dodge != R::Dodge::None && evade > 0)
            {
                Pose dodge{};
                const bool back = std::abs(pose.EvadeSide) < .5f;
                Dodges(pose.Dodge, back, evade, pose.Time, dodge);
                if (!back && pose.EvadeSide < 0) Mirror(dodge);
                Add(a, dodge);
            }

            // ---- hit: the chest and head are knocked back, the arms fly out
            a[R::Chest].X -= .35f * recoil; a[R::Head].X -= .30f * recoil;
            a[R::ArmL].Z -= .35f * recoil;  a[R::ArmR].Z += .35f * recoil;
            // ---- dazed (2026-11-19: doomed by her, waiting for her finishing kill): it sags, its head hangs, its arms
            //      are slack, its knees soft, and it sways
            const float sway = std::sin(pose.Time * 1.9f);
            a[R::Chest].X += .25f * daze;   a[R::Head].X += .45f * daze;
            a[R::Chest].Z += .06f * sway * daze; a[R::Head].Z += .09f * std::sin(pose.Time * 1.9f - .6f) * daze;
            a[R::ForeL].X += .20f * daze;   a[R::ForeR].X += .20f * daze;
            a[R::ArmL].Z += .08f * daze;    a[R::ArmR].Z -= .08f * daze;
            a[R::LegL].X -= .10f * daze;    a[R::ShinL].X += .20f * daze;
            a[R::LegR].X -= .10f * daze;    a[R::ShinR].X += .20f * daze;
            // ---- on its knees: its thighs upright, its shins back along the ground (KneelShin), bowed, arms hanging
            a[R::LegL].X -= .15f * kneel;   a[R::ShinL].X += 1.65f * kneel;
            a[R::LegR].X -= .15f * kneel;   a[R::ShinR].X += 1.65f * kneel;
            a[R::Chest].X += .30f * kneel;  a[R::Head].X += .15f * kneel;
            a[R::ArmL].X += .10f * kneel;   a[R::ArmR].X += .10f * kneel;
            a[R::ForeL].X -= .20f * kneel;  a[R::ForeR].X -= .20f * kneel;
            // ---- dying: the knees give way, the arms are flung up, the head drops back
            a[R::Chest].X -= .45f * death;  a[R::Head].X -= .40f * death;
            a[R::ArmL].X -= 1.1f * death;   a[R::ArmR].X -= 1.1f * death;
            a[R::ArmL].Z -= .50f * death;   a[R::ArmR].Z += .50f * death;
            a[R::LegL].X -= .50f * death;   a[R::ShinL].X += .95f * death;
            a[R::LegR].X -= .35f * death;   a[R::ShinR].X += .80f * death;
        }
        else if (rig.Kind == R::Type::Serpent)
        {   // a wave runs up the neck, and as it goes its neck winds with its going (Stride); its blows: it draws back and whips forward (Bite),
            // sweeps its head from its right to its left (Spray, through the action), rears up high and crashes down
            // (Rear); its dodge: the neck pulled back and aside (Sway)
            const int chain[] = {R::Chest, R::Spine2, R::Spine3, R::Head};
            const float h = pose.Dodge == R::Dodge::Sway ? Arc(evade) : 0.f, side = pose.EvadeSide < -.5f ? -1.f : 1.f;
            const R::Attack attack = drawn > 0 || act > 0 ? pose.Attack : pose.Struck;   // (only striking: the blow that landed)
            for (int k = 0; k < 4; ++k)
            {
                Angles& j = a[chain[k]];
                const float link = static_cast<float>(k);
                j.Y = .11f * std::sin(pose.Time * 1.4f - link * .9f) * (1 + walk) + .14f * g.Slither * walk * std::sin(pose.Stride - link * .9f);
                j.Z = .06f * std::sin(pose.Time * 1.1f - link * .7f);
                j.X = .22f * std::clamp(pose.Daze + pose.Kneel, 0.f, 1.f) - .10f * recoil + .35f * death;
                switch (attack)
                {
                case R::Attack::Spray:
                    j.X += .08f * drawn + Blow(strike, .08f, 0);
                    j.Y += (pose.Mirror ? -1.f : 1.f) * (.22f * drawn * (1 - act) + Lerp(.22f, -.26f, Ease(act)) * (act > 0 ? 1.f : 0.f) + Blow(strike, -.26f, 0));
                    break;
                case R::Attack::Rear:
                    j.X += -.42f * drawn + Blow(strike, -.42f, .50f) + .03f * std::sin(pose.Time * 7.f) * drawn;
                    break;
                default:   // Bite
                    j.X += -.16f * drawn + Blow(strike, -.16f, .30f);
                    break;
                }
                j.X += -.32f * h; j.Y += .28f * h * side;
            }
            a[R::Head].X += -.10f * drawn * (pose.Attack == R::Attack::Rear ? 1.f : 0.f);   // (rearing, it looks down at her)
        }
        else if (rig.Kind == R::Type::Winged)
        {   // wings beat, harder as it flies faster; they open wide before a blow and clap down with it (its dive), are
            // thrown forward (Gust), are swept back (Swoop, through the action); its dodge: one hard beat, banking (AirStrafe)
            const float rate = 4.5f, harder = 1 + .4f * walk;   // (a beat whose rate followed its speed would jump)
            float beat = .30f * harder * std::sin(pose.Time * rate), tip = .26f * harder * std::sin(pose.Time * rate - .9f);
            float open = .5f * death + .4f * std::clamp(pose.Daze, 0.f, 1.f);   // (dazed: they droop)
            float bow = 0;
            const R::Attack attack = drawn > 0 || act > 0 ? pose.Attack : pose.Struck;   // (only striking: the blow that landed)
            switch (attack)
            {
            case R::Attack::Gust:  open += -.70f * drawn + Blow(strike, -.70f, .85f); bow = -.20f * drawn + Blow(strike, -.20f, .30f); break;
            case R::Attack::Swoop:
            {
                const float folded = (std::max)(drawn, act > 0 && act < 1 ? 1.f : 0.f);
                open += .60f * folded + Blow(strike, .60f, -.40f); bow = .35f * folded + Blow(strike, .35f, -.15f);
                beat *= 1 - .8f * folded; tip *= 1 - .8f * folded;
                break;
            }
            default: open += -.45f * drawn + Blow(strike, -.45f, .65f); break;
            }
            {   // down on the ground (the Eagle, winded after its dive): its wings folded to its sides, beating no more
                const float perch = std::clamp(pose.Perch, 0.f, 1.f);
                beat *= 1 - .9f * perch; tip *= 1 - .9f * perch; open += .25f * perch; bow += .12f * perch;
            }
            if (pose.Dodge == R::Dodge::AirStrafe && evade > 0)
            {   // one hard beat down, banking into it
                const float h = Arc(evade);
                open += .75f * std::sin(Pi * std::clamp(evade * 1.4f, 0.f, 1.f));
                a[R::Chest].Z += -.30f * h * (pose.EvadeSide < 0 ? -1.f : 1.f);
            }
            a[R::WingL].Z = beat + open;     a[R::WingL2].Z = tip + .5f * open;
            a[R::WingR].Z = -(beat + open);  a[R::WingR2].Z = -(tip + .5f * open);
            {   // perched: its wings swept back along its body, their tips folded in behind it
                const float perch = std::clamp(pose.Perch, 0.f, 1.f);
                a[R::WingL].Y = -1.25f * perch; a[R::WingL2].Y = -1.1f * perch;
                a[R::WingR].Y = 1.25f * perch;  a[R::WingR2].Y = 1.1f * perch;
            }
            a[R::Chest].X = .05f * std::sin(pose.Time * rate + 1.2f) - .15f * recoil + bow;
        }

        // each joint turns about its pivot, then follows its parent
        XMMATRIX global[R::JointCount];
        for (int n = 0; n < R::JointCount; ++n)
        {
            const int j = rig.Order[n];
            const XMFLOAT3& p = rig.Pivot[j];
            const XMMATRIX local = XMMatrixTranslation(-p.x, -p.y, -p.z) *
                                   XMMatrixRotationX(a[j].X) * XMMatrixRotationY(a[j].Y) * XMMatrixRotationZ(a[j].Z) *
                                   XMMatrixTranslation(p.x, p.y, p.z);
            global[j] = rig.Parent[j] >= 0 ? local * global[rig.Parent[j]] : local;
        }
        // ---- its feet on the ground (two-legged; 2026-11-20): bent knees, a crouch, a squat, a stride lift its soles;
        //      it is let down so that the lower one stands where it stood (a sole: under its knee, at the model's
        //      lowest point). Dazed, kneeling or dying it is not: KneelDrop and the dying's sinking let those down. Off
        //      the ground (a hop, a leap) its tucked feet are let go of: its owner lifts it.
        float let = 0;
        const float held = (1 - down) * (1 - std::clamp(pose.Air, 0.f, 1.f));
        if (rig.Kind == R::Type::Biped && held > 0)
        {
            const auto sole = [&](int shin)
            {
                const XMFLOAT3 at{rig.Pivot[shin].x, rig.Sole, rig.Pivot[shin].z};
                return XMVectorGetY(XMVector3TransformCoord(XMLoadFloat3(&at), global[shin]));
            };
            let = ((std::min)(sole(R::ShinL), sole(R::ShinR)) - rig.Sole) * held;
            if (let != 0)
            {
                const XMMATRIX lower = XMMatrixTranslation(0, -let, 0);
                for (auto& m : global) m = m * lower;
            }
        }
        if (drop) *drop = let;
        out.JointCount = R::JointCount;
        for (int j = 0; j < R::JointCount; ++j) XMStoreFloat4x4(&out.FinalSkinningMatrices[j], global[j]);
    }
}

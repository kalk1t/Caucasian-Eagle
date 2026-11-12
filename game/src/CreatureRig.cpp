#include "CreatureRig.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

using namespace DirectX;

namespace CaucasianEagle
{
    namespace
    {
        // Angles are radians. About x: positive tips the top of a part backwards (a hanging arm or leg swings back),
        // negative swings it forward. About y: positive turns the right side back. About z: roll.
        struct Angles { float X = 0, Y = 0, Z = 0; };

        float Lerp(float a, float b, float t) { return a + (b - a) * t; }
        float Ease(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3 - 2 * t); }

        // The blow itself. 'strike' falls from 1 to 0: over the first quarter the part sweeps from where the windup
        // left it ('drawn') to the end of the blow ('hit'), then it returns to rest.
        float Blow(float strike, float drawn, float hit)
        {
            return strike > .75f ? Lerp(hit, drawn, (strike - .75f) / .25f) : hit * Ease(strike / .75f);
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

    void BuildCreaturePalette(const CreatureRig& rig, const CreaturePose& pose, Doorways::DoorwaysGpuSkinningPalette& out)
    {
        using R = CreatureRig;
        Angles a[R::JointCount];
        const float walk = pose.Walk, s = std::sin(pose.Stride), c = std::cos(pose.Stride);
        const float breath = std::sin(pose.Time * 1.7f);
        const float drawn = Ease(pose.Windup), strike = std::clamp(pose.Strike, 0.f, 1.f);
        const float recoil = std::clamp(pose.Recoil, 0.f, 1.f), death = Ease(pose.Death);

        if (rig.Kind == R::Type::Biped || rig.Kind == R::Type::Robed)
        {
            // ---- walking: legs swing against each other, the knee of the leg coming forward bends, arms swing
            //      against the legs, hips and shoulders twist against each other
            a[R::LegL].X = -.55f * walk * s;  a[R::ShinL].X = .80f * walk * std::max(0.f, c);
            a[R::LegR].X = .55f * walk * s;   a[R::ShinR].X = .80f * walk * std::max(0.f, -c);
            a[R::ArmL].X = .45f * walk * s;   a[R::ArmR].X = -.45f * walk * s;
            a[R::ForeL].X = a[R::ForeR].X = -.12f - .22f * walk;
            a[R::Pelvis].Y = .10f * walk * s; a[R::Pelvis].Z = .03f * walk * c;
            a[R::Chest].Y = -.22f * walk * s; a[R::Chest].X = .10f * walk + .025f * breath;
            a[R::Head].Y = .12f * walk * s;   a[R::Head].X = -.06f * walk - .02f * breath;
            // ---- standing: breathing moves the arms a little
            a[R::ArmL].Z = -.04f * breath * (1 - walk); a[R::ArmR].Z = .04f * breath * (1 - walk);
            // ---- the cloth of the robed spirits trails behind and sways
            a[R::Skirt1].X = .22f * walk + .05f * std::sin(pose.Time * 2.1f);       a[R::Skirt1].Z = .04f * std::sin(pose.Time * 1.3f);
            a[R::Skirt2].X = .30f * walk + .08f * std::sin(pose.Time * 2.1f - 1.f); a[R::Skirt2].Z = .06f * std::sin(pose.Time * 1.3f - .8f);

            // ---- the blow
            if (pose.Attack == R::Attack::Swing)
            {   // the right arm goes up and back over the shoulder, the body turns away, then everything comes through
                a[R::Chest].X += -.30f * drawn + Blow(strike, -.30f, .55f);
                a[R::Chest].Y += .45f * drawn + Blow(strike, .45f, -.50f);
                a[R::ArmR].X += -2.4f * drawn + Blow(strike, -2.4f, -.70f);
                a[R::ForeR].X += -.50f * drawn + Blow(strike, -.50f, -.10f);
                a[R::ArmL].X += -.50f * drawn + Blow(strike, -.50f, .50f);
                a[R::LegL].X += -.25f * drawn + Blow(strike, -.25f, -.45f);
                a[R::LegR].X += .20f * drawn + Blow(strike, .20f, .40f);
            }
            else if (pose.Attack == R::Attack::Slam)
            {   // both arms over the head, the back arched, then down
                a[R::Chest].X += -.42f * drawn + Blow(strike, -.42f, .70f);
                a[R::ArmL].X += -2.6f * drawn + Blow(strike, -2.6f, -.80f);
                a[R::ArmR].X += -2.6f * drawn + Blow(strike, -2.6f, -.80f);
                a[R::ForeL].X += -.40f * drawn; a[R::ForeR].X += -.40f * drawn;
                a[R::LegL].X += Blow(strike, 0, -.30f); a[R::ShinL].X += Blow(strike, 0, .35f);
                a[R::LegR].X += Blow(strike, 0, -.30f); a[R::ShinR].X += Blow(strike, 0, .35f);
            }
            else
            {   // hands drawn back and thrust forward, the cloth thrown back
                a[R::Chest].X += -.20f * drawn + Blow(strike, -.20f, .30f);
                a[R::ArmL].X += -.70f * drawn + Blow(strike, -.70f, -1.5f); a[R::ArmL].Z += -.35f * drawn;
                a[R::ArmR].X += -.70f * drawn + Blow(strike, -.70f, -1.5f); a[R::ArmR].Z += .35f * drawn;
                a[R::ForeL].X += -.80f * drawn; a[R::ForeR].X += -.80f * drawn;
                a[R::Skirt1].X += Blow(strike, 0, .25f); a[R::Skirt2].X += Blow(strike, 0, .35f);
            }
            a[R::Head].X += .15f * drawn;   // keeps its eyes on her while the body leans back

            // ---- hit: the chest and head are knocked back, the arms fly out
            a[R::Chest].X -= .35f * recoil; a[R::Head].X -= .30f * recoil;
            a[R::ArmL].Z -= .35f * recoil;  a[R::ArmR].Z += .35f * recoil;
            // ---- dying: the knees give way, the arms are flung up, the head drops back
            a[R::Chest].X -= .45f * death;  a[R::Head].X -= .40f * death;
            a[R::ArmL].X -= 1.1f * death;   a[R::ArmR].X -= 1.1f * death;
            a[R::ArmL].Z -= .50f * death;   a[R::ArmR].Z += .50f * death;
            a[R::LegL].X -= .50f * death;   a[R::ShinL].X += .95f * death;
            a[R::LegR].X -= .35f * death;   a[R::ShinR].X += .80f * death;
        }
        else if (rig.Kind == R::Type::Serpent)
        {   // a wave runs up the neck; it draws back, then whips forward and down
            const int chain[] = {R::Chest, R::Spine2, R::Spine3, R::Head};
            for (int k = 0; k < 4; ++k)
            {
                Angles& j = a[chain[k]];
                j.Y = .11f * std::sin(pose.Time * 1.4f - static_cast<float>(k) * .9f) * (1 + walk);
                j.Z = .06f * std::sin(pose.Time * 1.1f - static_cast<float>(k) * .7f);
                j.X = -.16f * drawn + Blow(strike, -.16f, .30f) - .10f * recoil + .35f * death;
            }
        }
        else if (rig.Kind == R::Type::Winged)
        {   // wings beat; they open wide before a blow and clap down with it
            const float beat = .30f * std::sin(pose.Time * 4.5f), tip = .26f * std::sin(pose.Time * 4.5f - .9f);
            const float open = -.45f * drawn + Blow(strike, -.45f, .65f) + .5f * death;
            a[R::WingL].Z = beat + open;     a[R::WingL2].Z = tip + .5f * open;
            a[R::WingR].Z = -(beat + open);  a[R::WingR2].Z = -(tip + .5f * open);
            a[R::Chest].X = .05f * std::sin(pose.Time * 4.5f + 1.2f) - .15f * recoil;
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
        out.JointCount = R::JointCount;
        for (int j = 0; j < R::JointCount; ++j) XMStoreFloat4x4(&out.FinalSkinningMatrices[j], global[j]);
    }
}

# Session 31 (0.1.8): makes the pictures' script for the baked hand places from grips370.txt: every weapon's block
# moved to a 600-frame cycle (SH3_EQUIP_EVERY=600) and, after its one-handed pictures, three more with the trigger finger
# off the trigger (1closeoff, 1rightoff, 1downoff); on the guns with a trigger of their own (handgun, shotgun, submachine
# gun) then the trigger pulled (owner message 151): from the side at 30%, 58% (just short of a shot) and all the way
# (1pull30, 1pull58, 1pull100), all the way from the right and from the eyes (1rightpull100, 1eyespull100), then close
# from the left, where the trigger blade is not behind the finger, resting, at 58% and all the way (1leftpull0,
# 1leftpull58, 1leftpull100).
# Run: awk -f tools/sim/make_pics.awk tools/sim/grips370.txt
BEGIN { k = -1; first = 7316; old_every = 400; new_every = 600 }
function out(d, line) { printf "%d %s\n", first + new_every * k + d, line }
function close_view(d) {
    out(d, "view_free 1")
    out(d, "view_x 0.26")
    out(d, "view_y -0.12")
    out(d, "view_z -0.27")
    out(d, "view_yaw 90")
    out(d, "view_pitch 0")
}
/^#/ {
    if ($0 ~ /^# --- /) {
        k++
        name = $3
        printf "# --- %s (equipped about %d)\n", name, first + new_every * k
        added = 0
    }
    next
}
NF == 0 { next }
{
    t = $1 + 0
    if (k < 0) { print; next }
    d = t - (first + old_every * k)
    rest = substr($0, index($0, " ") + 1)
    if (d >= 180 && !added) {
        tag = "g" k "-" name
        out(180, "right_first_touch 0")
        close_view(180)
        out(200, "shot " tag "-1closeoff")
        out(205, "view_x 0.38")
        out(205, "view_y -0.11")
        out(205, "view_z -0.25")
        out(215, "shot " tag "-1rightoff")
        out(220, "view_x 0")
        out(220, "view_y 0")
        out(220, "view_z 0")
        out(220, "view_yaw 0")
        out(220, "view_pitch -35")
        out(230, "shot " tag "-1downoff")
        out(235, "right_first_touch 1")
        if (name == "handgun" || name == "shotgun" || name == "smg") {
            close_view(235)
            out(245, "right_trigger 0.3")
            out(258, "shot " tag "-1pull30")
            out(261, "right_trigger 0.58")
            out(274, "shot " tag "-1pull58")
            out(277, "right_trigger 1")
            out(290, "shot " tag "-1pull100")
            out(293, "view_x 0.38")
            out(293, "view_y -0.11")
            out(293, "view_z -0.25")
            out(303, "shot " tag "-1rightpull100")
            out(306, "view_free 0")
            out(306, "view_yaw 0")
            out(306, "view_pitch 0")
            out(316, "shot " tag "-1eyespull100")
            out(320, "right_trigger 0")
            out(320, "view_free 1")
            out(320, "view_x -0.1")
            out(320, "view_y -0.11")
            out(320, "view_z -0.25")
            out(320, "view_yaw -90")
            out(320, "view_pitch 0")
            out(333, "shot " tag "-1leftpull0")
            out(336, "right_trigger 0.58")
            out(349, "shot " tag "-1leftpull58")
            out(352, "right_trigger 1")
            out(365, "shot " tag "-1leftpull100")
            out(368, "right_trigger 0")
        }
        out(372, "view_free 0")
        out(372, "view_yaw 0")
        out(372, "view_pitch 0")
        added = 1
    }
    if (d >= 180) d += 200
    out(d, rest)
}

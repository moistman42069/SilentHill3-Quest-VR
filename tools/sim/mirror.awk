# Session 31 (0.1.8): a rig script mirrored for a left-handed player (left_handed=1 in the rig's sh3vr.ini): the
# controllers' roles swapped (rh_ <-> lh_, right_ <-> left_ for the triggers, grips and touches, lh_at_second ->
# rh_at_second), every x and yaw across the body negated (hands and free views), the pictures named L... instead of g...
# Run: awk -f tools/sim/mirror.awk tools/sim/pics383.txt > tools/sim/lefty384.txt
function swap(w) {
    if (w ~ /^rh_/) return "lh_" substr(w, 4)
    if (w ~ /^lh_at_second$/) return "rh_at_second"
    if (w ~ /^lh_/) return "rh_" substr(w, 4)
    if (w ~ /^right_/) return "left_" substr(w, 7)
    if (w ~ /^left_/) return "right_" substr(w, 6)
    return w
}
/^#/ { print; next }
NF == 0 { print; next }
{
    key = $2
    val = $3
    if (key == "shot") { sub(/^g/, "L", val); print $1, key, val; next }
    k2 = swap(key)
    if (k2 ~ /_x$/ || key == "view_yaw") {
        v = -(val + 0)
        if (v == 0) v = 0
        print $1, k2, v
        next
    }
    print $1, k2, val
}

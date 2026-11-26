// SPDX-License-Identifier: GPL-3.0-or-later
package dev.sh3quest.vr;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;
import java.util.zip.CRC32;

/**
 * 0.1.4 (owner 90: "someone also said it keeps saying they have the wrong version installed. then asked if it works
 * with the european version"; owner 91: "if anything, try to extend compatibility however you can"): is a player's
 * sh3.exe the game's 2003-09-03 build, whatever was done to its code?
 *
 * <p>The port runs its own recompiled copy of the game's code, so what it needs of sh3.exe is the game's data at the
 * build's addresses. A disc's copy protection, a no-CD/no-DVD patch or a fix changes the code (and a protection the
 * import tables, which the loader fills from the build's own list, src/runtime/exe_ref.h), so the exe's SHA-1 cannot
 * say. This compares its data sections, page by page, with the build's CRC-32s (game_files.txt's base, section, skip
 * and page lines, tools/exe_ref.py). Plain Java (no Android), so it can be tried on a PC.
 */
final class ExeCheck {
    private ExeCheck() {}

    static final int PAGE = 4096;
    /** Data pages that may differ (a patch to the game's data); another build differs in nearly all of them. */
    static final int MAX_PAGES_CHANGED = 16;

    /** The build's data, from game_files.txt (tools/exe_ref.py); numbers are hex, addresses relative to the base. */
    static final class Reference {
        long base = -1, sizeOfImage;
        final List<long[]> sections = new ArrayList<>();  // rva, virtual size, bytes checked
        final List<long[]> skips = new ArrayList<>();     // rva, size: checked as zeros
        final List<long[]> pages = new ArrayList<>();     // rva, length, crc32

        /** Takes one manifest line, split on spaces; false when it is not one of these. */
        boolean add(String[] p) {
            try {
                switch (p[0]) {
                    case "base":
                        base = Long.parseLong(p[1], 16);
                        sizeOfImage = Long.parseLong(p[2], 16);
                        return true;
                    case "section":
                        sections.add(new long[] {Long.parseLong(p[2], 16), Long.parseLong(p[3], 16), Long.parseLong(p[4], 16)});
                        return true;
                    case "skip":
                        skips.add(new long[] {Long.parseLong(p[1], 16), Long.parseLong(p[2], 16)});
                        return true;
                    case "page":
                        pages.add(new long[] {Long.parseLong(p[1], 16), Long.parseLong(p[2], 16), Long.parseLong(p[3], 16)});
                        return true;
                    default:
                        return false;
                }
            } catch (RuntimeException e) {
                return p.length > 0 && (p[0].equals("base") || p[0].equals("section") || p[0].equals("skip")
                    || p[0].equals("page"));
            }
        }

        boolean usable() {
            return base >= 0 && !pages.isEmpty();
        }
    }

    /** What the exe is. */
    static final class Result {
        boolean windowsProgram;
        long size, timestamp, base, sizeOfImage;
        String sectionNames = "";
        boolean layout;          // the build's data sections at their addresses
        int pagesChecked, pagesChanged;
        long firstChanged = -1;  // rva
        String why = "";         // when it is not the build: why, in words

        boolean sameBuild() {
            return windowsProgram && layout && pagesChecked > 0 && pagesChanged <= MAX_PAGES_CHANGED;
        }

        String built() {
            if (timestamp == 0) {
                return "?";
            }
            SimpleDateFormat f = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
            f.setTimeZone(TimeZone.getTimeZone("UTC"));
            return f.format(new Date(timestamp * 1000L));
        }
    }

    private static int u16(byte[] d, long o) {
        if (o < 0 || o + 2 > d.length) {
            throw new IndexOutOfBoundsException();
        }
        int i = (int) o;
        return (d[i] & 0xFF) | (d[i + 1] & 0xFF) << 8;
    }

    private static long u32(byte[] d, long o) {
        if (o < 0 || o + 4 > d.length) {
            throw new IndexOutOfBoundsException();
        }
        int i = (int) o;
        return (d[i] & 0xFFL) | (d[i + 1] & 0xFFL) << 8 | (d[i + 2] & 0xFFL) << 16 | (d[i + 3] & 0xFFL) << 24;
    }

    /** The exe's sections: va, virtual size, raw size, raw offset (as the loader maps them). */
    private static List<long[]> sections(byte[] d, long[] header, StringBuilder names) {
        long pe = u32(d, 0x3C);
        if (u32(d, pe) != 0x00004550L) {  // "PE\0\0"
            throw new IllegalArgumentException("no PE header");
        }
        int count = u16(d, pe + 6);
        header[0] = u32(d, pe + 8);           // timestamp
        long opt = pe + 24;
        header[1] = u32(d, opt + 28);         // image base
        header[2] = u32(d, opt + 56);         // size of image
        long at = opt + u16(d, pe + 20);
        List<long[]> out = new ArrayList<>();
        for (int i = 0; i < count && i < 96; i++, at += 40) {
            StringBuilder name = new StringBuilder();
            for (int k = 0; k < 8; k++) {
                int c = d[(int) (at + k)] & 0xFF;
                if (c == 0) {
                    break;
                }
                name.append(c >= 32 && c < 127 ? (char) c : '?');
            }
            names.append(i > 0 ? " " : "").append(name);
            out.add(new long[] {u32(d, at + 12), u32(d, at + 8), u32(d, at + 16), u32(d, at + 20)});
        }
        return out;
    }


    /** The image's bytes [rva, rva + n) as the loader maps them: past a section's raw data, zeros. */
    private static byte[] image(byte[] d, List<long[]> secs, long rva, int n) {
        byte[] out = new byte[n];
        for (long[] s : secs) {
            long va = s[0], vs = s[1], rs = s[2], ro = s[3];
            long lo = Math.max(rva, va), hi = Math.min(rva + n, va + Math.min(rs, vs));
            for (long a = lo; a < hi; a++) {
                long o = ro + (a - va);
                if (o >= 0 && o < d.length) {
                    out[(int) (a - rva)] = d[(int) o];
                }
            }
        }
        return out;
    }

    static Result compare(byte[] d, Reference ref) {
        Result r = new Result();
        r.size = d.length;
        List<long[]> secs;
        long[] header = new long[3];
        StringBuilder names = new StringBuilder();
        try {
            secs = sections(d, header, names);
        } catch (RuntimeException e) {
            r.why = "it is not a Windows program (a damaged copy, or another file named sh3.exe)";
            return r;
        }
        r.windowsProgram = true;
        r.timestamp = header[0];
        r.base = header[1];
        r.sizeOfImage = header[2];
        r.sectionNames = names.toString();
        if (r.base != ref.base) {
            r.why = "it is a program for another address (another build)";
            return r;
        }
        for (long[] want : ref.sections) {
            long rva = want[0], vsize = want[1], checked = want[2];
            boolean covered = false;
            for (long[] s : secs) {
                if (s[0] <= rva && s[0] + Math.max(s[1], s[2]) >= rva + checked) {
                    covered = true;
                }
            }
            if (!covered || r.sizeOfImage < rva + vsize) {
                r.why = "its data is laid out differently (another build)";
                return r;
            }
        }
        r.layout = true;
        CRC32 crc = new CRC32();
        for (long[] p : ref.pages) {
            long rva = p[0];
            int n = (int) p[1];
            byte[] b = image(d, secs, rva, n);
            for (long[] s : ref.skips) {
                long lo = Math.max(rva, s[0]), hi = Math.min(rva + n, s[0] + s[1]);
                for (long a = lo; a < hi; a++) {
                    b[(int) (a - rva)] = 0;
                }
            }
            crc.reset();
            crc.update(b, 0, n);
            r.pagesChecked++;
            if (crc.getValue() != p[2]) {
                r.pagesChanged++;
                if (r.firstChanged < 0) {
                    r.firstChanged = rva;
                }
            }
        }
        if (r.pagesChanged > MAX_PAGES_CHANGED) {
            r.why = "its data differs from the 2003-09-03 build's in " + r.pagesChanged + " of " + r.pagesChecked
                + " places (another build, or a copy still encrypted by its disc's copy protection)";
        }
        return r;
    }
}

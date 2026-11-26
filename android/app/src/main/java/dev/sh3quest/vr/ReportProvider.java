// SPDX-License-Identifier: GPL-3.0-or-later
// After UpdateProvider (itself after the Halo CE Decomp Remix launcher's, moistman42069).
package dev.sh3quest.vr;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Hands the app the player shares a report with (the Logs tab's Share) the one file
 * content://dev.sh3quest.vr.report/sh3quest-report.txt, read-only, for that share only.
 */
public final class ReportProvider extends ContentProvider {
    static final String AUTHORITY = BuildConfig.APPLICATION_ID + ".report";
    static final String NAME = "sh3quest-report.txt";
    static final Uri URI = Uri.parse("content://" + AUTHORITY + "/" + NAME);

    static File file(Context context) {
        return new File(new File(context.getCacheDir(), "report"), NAME);
    }

    private boolean isReport(Uri uri) {
        return uri != null && "content".equals(uri.getScheme()) && AUTHORITY.equals(uri.getAuthority())
            && ("/" + NAME).equals(uri.getPath()) && uri.getQuery() == null && uri.getFragment() == null;
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!isReport(uri) || !"r".equals(mode)) {
            throw new FileNotFoundException(String.valueOf(uri));
        }
        return ParcelFileDescriptor.open(file(getContext()), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public String getType(Uri uri) {
        return isReport(uri) ? "text/plain" : null;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        if (!isReport(uri)) {
            return null;
        }
        MatrixCursor cursor = new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {NAME, file(getContext()).length()});
        return cursor;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}

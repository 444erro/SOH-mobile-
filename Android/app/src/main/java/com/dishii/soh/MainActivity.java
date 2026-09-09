
package com.dishii.soh;
import org.libsdl.app.SDLActivity;

import android.content.Context;
import android.content.ClipData;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.database.Cursor;
import android.net.Uri;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.Manifest;
import android.os.Bundle;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.provider.OpenableColumns;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;

import android.view.WindowManager;
import android.widget.Toast;

import android.util.Log;

import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageView;
import android.widget.EditText;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.SeekBar;
import android.widget.ScrollView;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.InputFilter;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;

import java.util.concurrent.Executors;
import android.app.AlertDialog;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

//This class is the main SDLActivity and just sets up a bunch of default files
public class MainActivity extends SDLActivity{

    SharedPreferences preferences;
    private static final CountDownLatch setupLatch = new CountDownLatch(1);
    private View extractionLoadingView;

    public void showExtractionLoading() {
        runOnUiThread(() -> {
            if (extractionLoadingView != null) return;

            FrameLayout root = findViewById(android.R.id.content);
            FrameLayout overlay = new FrameLayout(this);
            overlay.setBackgroundColor(Color.rgb(12, 15, 20));
            overlay.setClickable(true);

            android.widget.LinearLayout content = new android.widget.LinearLayout(this);
            content.setOrientation(android.widget.LinearLayout.VERTICAL);
            content.setGravity(Gravity.CENTER);
            int padding = (int) (32 * getResources().getDisplayMetrics().density);
            content.setPadding(padding, padding, padding, padding);

            TextView title = new TextView(this);
            title.setText("SOH-Mobile-Anchor");
            title.setTextColor(Color.WHITE);
            title.setTextSize(24);
            title.setGravity(Gravity.CENTER);

            TextView status = new TextView(this);
            status.setText("Extraindo arquivos OTR...\nNão feche o aplicativo.");
            status.setTextColor(Color.LTGRAY);
            status.setTextSize(16);
            status.setGravity(Gravity.CENTER);
            status.setPadding(0, padding / 2, 0, padding / 2);

            ProgressBar progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
            progress.setIndeterminate(true);

            content.addView(title, new android.widget.LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            content.addView(status, new android.widget.LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            content.addView(progress, new android.widget.LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            overlay.addView(content, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            root.addView(overlay, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
            extractionLoadingView = overlay;
        });
    }

    public void hideExtractionLoading() {
        runOnUiThread(() -> {
            if (extractionLoadingView != null) {
                ViewGroup parent = (ViewGroup) extractionLoadingView.getParent();
                if (parent != null) parent.removeView(extractionLoadingView);
                extractionLoadingView = null;
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        preferences = getSharedPreferences("com.dishii.soh.prefs",Context.MODE_PRIVATE);

        if (hasStoragePermission()) {
            doVersionCheck();
            checkAndSetupFiles();
        } else {
            requestStoragePermission();
        }

        super.onCreate(savedInstanceState);

        setupControllerOverlay();
        attachController();
    }

    public static void waitForSetupFromNative() {
        try {
            setupLatch.await();  // Block until setup is complete
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    private void doVersionCheck(){
        int currentVersion = BuildConfig.VERSION_CODE;
        int storedVersion = preferences.getInt("appVersion", 1);

        if (currentVersion > storedVersion) {
            deleteOutdatedAssets();
            preferences.edit().putInt("appVersion", currentVersion).apply();
        }
    }

    private void deleteOutdatedAssets() {
        File targetRootFolder = getAppDataDirectory();

        File sohFile = new File(targetRootFolder, "soh.otr");
        File ootFile = new File(targetRootFolder, "oot.otr");
        File ootMqFile = new File(targetRootFolder, "oot-mq.otr");
        File assetsFolder = new File(targetRootFolder, "assets");

        deleteIfExists(sohFile);
        deleteIfExists(ootFile);
        deleteIfExists(ootMqFile);
        deleteRecursiveIfExists(assetsFolder);
    }

    private void deleteIfExists(File file) {
        if (file.exists()) {
            if (file.delete()) {
                Log.i("deleteAssets", "Deleted outdated app file");
            } else {
                Log.w("deleteAssets", "Failed to delete outdated app file");
            }
        } else {
            Log.i("deleteAssets", "Outdated app file not present");
        }
    }

    private void deleteRecursiveIfExists(File dir) {
        if (dir.exists()) {
            deleteRecursive(dir);
            Log.i("deleteAssets", "Deleted outdated app directory");
        } else {
            Log.i("deleteAssets", "Outdated app directory not present");
        }
    }

    private void deleteRecursive(File fileOrDirectory) {
        if (fileOrDirectory.isDirectory()) {
            File[] children = fileOrDirectory.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursive(child);
                }
            }
        }
        fileOrDirectory.delete();
    }



    private static final int FILE_PICKER_REQUEST_CODE = 0;
    private static final int MOD_PICKER_REQUEST_CODE = 1;
    private static final int STORAGE_PERMISSION_REQUEST_CODE = 2296;

    private File getAppDataDirectory() {
        if (hasStoragePermission()) {
            return new File(Environment.getExternalStorageDirectory(), "SOH");
        }
        File external = getExternalFilesDir(null);
        return external != null ? external : getFilesDir();
    }

    public String getSohDataDirectoryPath() {
        return getAppDataDirectory().getAbsolutePath();
    }

    public boolean isHeadsetAudioConnected() {
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (audioManager == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return false;
        }

        for (AudioDeviceInfo device : audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            switch (device.getType()) {
                case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
                case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                case AudioDeviceInfo.TYPE_USB_DEVICE:
                    return true;
                default:
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O &&
                            device.getType() == AudioDeviceInfo.TYPE_USB_HEADSET) {
                        return true;
                    }
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
                            device.getType() == AudioDeviceInfo.TYPE_BLE_HEADSET) {
                        return true;
                    }
                    break;
            }
        }
        return false;
    }

    private boolean hasStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return true;
        }
        return ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE)
                        == PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                        == PackageManager.PERMISSION_GRANTED;
    }

    private void requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            startActivityForResult(intent, STORAGE_PERMISSION_REQUEST_CODE);
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.READ_EXTERNAL_STORAGE,
                            Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    STORAGE_PERMISSION_REQUEST_CODE);
        } else {
            doVersionCheck();
            checkAndSetupFiles();
        }
    }

    private void continueSetupAfterStorageChoice() {
        doVersionCheck();
        checkAndSetupFiles();
        if (!hasStoragePermission()) {
            Toast.makeText(this,
                    "Without file access, SOH uses its protected folder instead of /SOH.",
                    Toast.LENGTH_LONG).show();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == STORAGE_PERMISSION_REQUEST_CODE) {
            continueSetupAfterStorageChoice();
        }
    }

    public void checkAndSetupFiles() {
        File targetRootFolder = getAppDataDirectory();
        ensureModsDirectory(targetRootFolder);
        migrateLegacyMods(targetRootFolder);
        File assetsFolder = new File(targetRootFolder, "assets");
        File sohOtrFile = new File(targetRootFolder, "soh.otr");

        boolean isMissingAssets = !assetsFolder.exists() || assetsFolder.listFiles() == null || assetsFolder.listFiles().length == 0;
        boolean isMissingSohOtr = !sohOtrFile.exists();

        if (!targetRootFolder.exists() || isMissingAssets || isMissingSohOtr) {
            new AlertDialog.Builder(this)
                    .setTitle("Setup Required")
                    .setMessage("Some required files are missing. The app will create them (~1 minute). Press OK to begin.")
                    .setCancelable(false)
                    .setPositiveButton("OK", (dialog, which) -> {
                        Executors.newSingleThreadExecutor().execute(() -> {
                            runOnUiThread(() -> Toast.makeText(this, "Setting up files...", Toast.LENGTH_SHORT).show());
                            setupFilesInBackground(targetRootFolder);
                        });
                    })
                    .show();
        } else {
            // No setup needed, still need to count down
            setupLatch.countDown();
        }
    }


    private void setupFilesInBackground(File targetRootFolder) {

        File sourceOldRoot = getExternalFilesDir(null);

        // Preserve saves, configuration and mods created while scoped storage
        // was in use when returning to the public /SOH directory.
        if (sourceOldRoot != null && !sourceOldRoot.equals(targetRootFolder) &&
                sourceOldRoot.canRead() && sourceOldRoot.isDirectory()) {
            Log.i("setupFiles", "Migrating protected app data");

            File[] sourceFiles = sourceOldRoot.listFiles();
            if (sourceFiles != null) {
                for (File file : sourceFiles) {
                    String name = file.getName();
                    if (name.equals("assets") || name.equals("soh.otr") || name.equals("oot-mq.otr") || name.equals("oot.otr")) {
                        continue; // Skip these
                    }

                    File dest = new File(targetRootFolder, name);
                    try {
                        if (file.isDirectory()) {
                            AssetCopyUtil.copyDirectory(file, dest);
                        } else {
                            AssetCopyUtil.copyFile(file, dest);
                        }
                        Log.i("setupFiles", "Migrated protected app entry");
                    } catch (IOException e) {
                        Log.e("setupFiles", "Failed to migrate protected app entry", e);
                    }
                }
            }

            runOnUiThread(() -> Toast.makeText(this, "Save data migrated", Toast.LENGTH_SHORT).show());
        }

        // Ensure root folder exists
        if (!targetRootFolder.exists()) {
            if (!targetRootFolder.mkdirs()) {
                Log.e("setupFiles", "Failed to create root folder");
                runOnUiThread(() -> Toast.makeText(this, "Failed to create folder", Toast.LENGTH_LONG).show());
                setupLatch.countDown();
                return;
            }
        }

        // Always ensure mods folder exists
        File targetModsDir = new File(targetRootFolder, "mods");
        if (!targetModsDir.exists()) {
            targetModsDir.mkdirs();
        }

        // Copy assets/ from internal
        File targetAssetsDir = new File(targetRootFolder, "assets");
        try {
            if (!targetAssetsDir.exists()) {
                targetAssetsDir.mkdirs();
            }
            AssetCopyUtil.copyAssetsToExternal(this, "assets", targetAssetsDir.getAbsolutePath());
            runOnUiThread(() -> Toast.makeText(this, "Assets copied", Toast.LENGTH_SHORT).show());
        } catch (IOException e) {
            e.printStackTrace();
            runOnUiThread(() -> Toast.makeText(this, "Error copying assets", Toast.LENGTH_LONG).show());
        }

        // Copy soh.otr from internal assets
        File targetOtrFile = new File(targetRootFolder, "soh.otr");
        try (InputStream in = getAssets().open("soh.otr");
             OutputStream out = new FileOutputStream(targetOtrFile)) {

            byte[] buffer = new byte[1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }

            runOnUiThread(() -> Toast.makeText(this, "soh.otr copied", Toast.LENGTH_SHORT).show());

        } catch (IOException e) {
            e.printStackTrace();
            runOnUiThread(() -> Toast.makeText(this, "Error copying soh.otr", Toast.LENGTH_LONG).show());
        }

        setupLatch.countDown();
    }

    private void ensureModsDirectory(File targetRootFolder) {
        if (!targetRootFolder.exists()) {
            targetRootFolder.mkdirs();
        }

        File targetModsDir = new File(targetRootFolder, "mods");
        if (!targetModsDir.exists() && !targetModsDir.mkdirs()) {
            Log.e("mods", "Failed to create protected mods directory");
            return;
        }

        // Older builds and some file managers used the singular folder name.
        File singularModsDir = new File(targetRootFolder, "mod");
        if (singularModsDir.isDirectory()) {
            try {
                AssetCopyUtil.copyDirectory(singularModsDir, targetModsDir);
                Log.i("mods", "Migrated singular mod directory");
            } catch (IOException e) {
                Log.e("mods", "Failed to migrate singular mod directory", e);
            }
        }
    }

    private void migrateLegacyMods(File targetRootFolder) {
        File targetModsDir = new File(targetRootFolder, "mods");
        File legacyRoot = new File(Environment.getExternalStorageDirectory(), "SOH");
        File[] legacyModDirs = {
                new File(legacyRoot, "mods"),
                new File(legacyRoot, "mod")
        };

        for (File legacyModDir : legacyModDirs) {
            if (legacyModDir.equals(targetModsDir) || !legacyModDir.isDirectory() || !legacyModDir.canRead()) {
                continue;
            }
            try {
                AssetCopyUtil.copyDirectory(legacyModDir, targetModsDir);
                Log.i("mods", "Migrated readable legacy mods");
            } catch (IOException e) {
                Log.e("mods", "Failed to migrate readable legacy mods", e);
            }
        }
    }




    private native void nativeHandleSelectedFile(String filePath);

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == FILE_PICKER_REQUEST_CODE && resultCode == RESULT_OK && data != null) {
            // Handle file selection
            Uri selectedFileUri = data.getData();
            String fileName = "OOT.z64";

            File destinationDirectory = getAppDataDirectory();
            File destinationFile = new File(destinationDirectory, fileName);

            if (selectedFileUri != null) {
                try (InputStream in = getContentResolver().openInputStream(selectedFileUri);
                     OutputStream out = new FileOutputStream(destinationFile)) {
                    if (in == null) {
                        throw new IOException("Unable to open selected document");
                    }

                    byte[] buffer = new byte[4096];
                    int bytesRead;
                    while ((bytesRead = in.read(buffer)) != -1) {
                        out.write(buffer, 0, bytesRead);
                    }
                } catch (IOException e) {
                    Log.e("filePicker", "Failed to import selected document", e);
                }
            }

            // Now pass the path of the file in the new folder
            nativeHandleSelectedFile(destinationFile.getPath());
        } else if (requestCode == MOD_PICKER_REQUEST_CODE && resultCode == RESULT_OK && data != null) {
            int imported = 0;
            ClipData selectedFiles = data.getClipData();
            if (selectedFiles != null) {
                for (int i = 0; i < selectedFiles.getItemCount(); i++) {
                    if (importModFile(selectedFiles.getItemAt(i).getUri())) {
                        imported++;
                    }
                }
            } else if (data.getData() != null && importModFile(data.getData())) {
                imported = 1;
            }

            final int importedCount = imported;
            Toast.makeText(this,
                    importedCount > 0
                            ? importedCount + " mod(s) imported. Restart the game to load them."
                            : "No supported mod file was imported.",
                    Toast.LENGTH_LONG).show();
        } else if (requestCode == STORAGE_PERMISSION_REQUEST_CODE) {
            continueSetupAfterStorageChoice();
        }
    }

    public void openFilePicker() {
        // Create an Intent to open the file picker dialog
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");

        // Start the file picker dialog
        startActivityForResult(intent, 0);
    }

    public void openModFilePicker() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
            startActivityForResult(intent, MOD_PICKER_REQUEST_CODE);
        });
    }

    private boolean importModFile(Uri sourceUri) {
        String displayName = getDisplayName(sourceUri);
        if (displayName == null) {
            return false;
        }

        // Strip path separators supplied by an untrusted document provider.
        displayName = displayName.replace('\\', '/');
        displayName = displayName.substring(displayName.lastIndexOf('/') + 1);
        String lowerName = displayName.toLowerCase(Locale.ROOT);
        if (!(lowerName.endsWith(".otr") || lowerName.endsWith(".o2r") ||
                lowerName.endsWith(".zip") || lowerName.endsWith(".mpq"))) {
            return false;
        }

        File modsDirectory = new File(getAppDataDirectory(), "mods");
        ensureModsDirectory(getAppDataDirectory());
        File destination = new File(modsDirectory, displayName);
        try (InputStream in = getContentResolver().openInputStream(sourceUri);
             OutputStream out = new FileOutputStream(destination)) {
            if (in == null) {
                return false;
            }
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            return true;
        } catch (IOException e) {
            Log.e("mods", "Failed to import mod file", e);
            return false;
        }
    }

    private String getDisplayName(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri,
                new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int nameColumn = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (nameColumn >= 0) {
                    return cursor.getString(nameColumn);
                }
            }
        } catch (RuntimeException e) {
            Log.e("mods", "Unable to read selected mod name", e);
        }
        return uri.getLastPathSegment();
    }

    // Check if external storage is available and writable
    private boolean isExternalStorageWritable() {
        String state = Environment.getExternalStorageState();
        return Environment.MEDIA_MOUNTED.equals(state);
    }

    public native void attachController();
    public native void detachController();
    // Native method for setting button state
    public native void setButton(int button, boolean value);
    public native void setCameraState(int axis, float value);

    // Native method for setting joystick axis value
    public native void setAxis(int axis, short value);
    public native void sendMultiplayerChat(String message);
    public native void setNativeMenuVisible(boolean visible);
    public native void toggleNativeMenu();
    public native int getNativeMenuLanguage();

    private Button button1, button2, button3, button4;
    private Button buttonA, buttonB, buttonX, buttonY;
    private Button buttonDpadUp, buttonDpadDown, buttonDpadLeft, buttonDpadRight;
    private Button buttonLB, buttonRB, buttonZ, buttonStart, buttonBack, buttonChat;
    private Button buttonToggle;
    private EditText chatInput;
    private FrameLayout leftJoystick;
    private FrameLayout rightScreenArea;
    private ImageView leftJoystickKnob;
    private ImageView cDpadCross;
    private View overlayView;
    private ViewGroup controllerButtonGroup;
    private android.widget.LinearLayout hudEditBar;
    private Button hudResetButton, hudApplyButton;
    private FrameLayout chatPanel;
    private android.widget.LinearLayout chatHistoryLayout;
    private android.widget.LinearLayout chatTabsLayout;
    private FrameLayout privateTabsPanel;
    private android.widget.LinearLayout privateTabsLayout;
    private ScrollView privateTabsScroll;
    private EditText chatPanelInput;
    private ScrollView chatHistoryScroll;
    private boolean chatPanelOpen = false;
    private String activePrivateTarget = "";
    private String pendingLocalPublicMessage = "";
    private long pendingLocalPublicMessageAtMs = 0L;
    private final List<ChatEntry> chatEntries = new ArrayList<>();
    private final Map<String, Integer> playerColors = new HashMap<>();
    private final List<String> privateTargets = new ArrayList<>();
    private final List<String> unreadPrivateSenders = new ArrayList<>();
    private final List<View> hudEditableViews = new ArrayList<>();
    private final List<View> cDpadViews = new ArrayList<>();
    private final Map<View, float[]> hudGroupDragStarts = new HashMap<>();
    private boolean hudEditMode = false;
    private float hudDragOffsetX = 0f;
    private float hudDragOffsetY = 0f;
    private float hudDragStartRawX = 0f;
    private float hudDragStartRawY = 0f;
    private boolean hudDragged = false;
    private static final float HUD_DRAG_CLICK_SLOP_DP = 8f;

    private static class ChatEntry {
        String player;
        String message;
        int color;
        String target;
        long createdAtMs;

        ChatEntry(String player, String message, int color, String target) {
            this.player = player;
            this.message = message;
            this.color = color;
            this.target = target;
            this.createdAtMs = System.currentTimeMillis();
        }
    }

    // Function to set up the controller overlay (inflate layout and initialize buttons)
    private void setupControllerOverlay() {
        // Inflate the touchcontrol_overlay layout
        LayoutInflater inflater = (LayoutInflater) getSystemService(LAYOUT_INFLATER_SERVICE);
        overlayView = inflater.inflate(R.layout.touchcontrol_overlay, null);

        // Set layout params for overlayView to control positioning and sizing
        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
        );
        overlayView.setLayoutParams(layoutParams);
        // Add overlay view to the main layout (you may need to add it to a container like FrameLayout)
        ViewGroup view = (ViewGroup) getContentView();
        view.addView(overlayView);
        view.setKeepScreenOn(true);

        final ViewGroup buttonGroup = overlayView.findViewById(R.id.button_group);
        controllerButtonGroup = buttonGroup;

        buttonA = overlayView.findViewById(R.id.buttonA);
        buttonB = overlayView.findViewById(R.id.buttonB);
        buttonX = overlayView.findViewById(R.id.buttonX);
        buttonY = overlayView.findViewById(R.id.buttonY);

        buttonDpadUp = overlayView.findViewById(R.id.buttonDpadUp);
        buttonDpadDown = overlayView.findViewById(R.id.buttonDpadDown);
        buttonDpadLeft = overlayView.findViewById(R.id.buttonDpadLeft);
        buttonDpadRight = overlayView.findViewById(R.id.buttonDpadRight);

        buttonLB = overlayView.findViewById(R.id.buttonLB);
        buttonRB = overlayView.findViewById(R.id.buttonRB);
        buttonZ = overlayView.findViewById(R.id.buttonZ);

        buttonStart = overlayView.findViewById(R.id.buttonStart);
        buttonBack = overlayView.findViewById(R.id.buttonBack);
        buttonChat = overlayView.findViewById(R.id.buttonChat);
        chatInput = overlayView.findViewById(R.id.chatInput);

        buttonToggle = overlayView.findViewById(R.id.buttonToggle);
        cDpadCross = overlayView.findViewById(R.id.imageView);

        // Initialize joysticks and joystick knobs from the inflated layout
        leftJoystick = overlayView.findViewById(R.id.left_joystick);
        leftJoystickKnob = overlayView.findViewById(R.id.left_joystick_knob);

        rightScreenArea = overlayView.findViewById(R.id.right_screen_area);
        registerHudEditableViews();

        // Set OnTouchListeners for the Xbox controller buttons
        addTouchListener(buttonA, ControllerButtons.BUTTON_A); // SDL Button 0 (A)
        addTouchListener(buttonB, ControllerButtons.BUTTON_B); // SDL Button 1 (B)
        addTouchListener(buttonX, ControllerButtons.BUTTON_X); // SDL Button 2 (X)
        addTouchListener(buttonY, ControllerButtons.BUTTON_Y); // SDL Button 3 (Y)

        setupCButtons(buttonDpadUp, ControllerButtons.AXIS_RY, 1); // SDL Button 10 (D-Pad Up)
        setupCButtons(buttonDpadDown, ControllerButtons.AXIS_RY , -1); // SDL Button 11 (D-Pad Down)
        setupCButtons(buttonDpadLeft, ControllerButtons.AXIS_RX, 1); // SDL Button 12 (D-Pad Left)
        setupCButtons(buttonDpadRight, ControllerButtons.AXIS_RX, -1); // SDL Button 13 (D-Pad Right)

        addTouchListener(buttonLB, ControllerButtons.BUTTON_LB); // SDL Button 4 (LB)
        addTouchListener(buttonRB, ControllerButtons.BUTTON_RB); // SDL Button 5 (RB)
        addTouchListener(buttonZ, ControllerButtons.AXIS_RT); // SDL Button 5 (Z)

        addTouchListener(buttonStart, ControllerButtons.BUTTON_START); // SDL Button 7 (Start)
        addTouchListener(buttonBack, ControllerButtons.BUTTON_BACK);
        buttonChat.setOnTouchListener((v, event) -> {
            if (hudEditMode) {
                return handleHudEditTouch(v, event);
            }
            if (event.getActionMasked() == MotionEvent.ACTION_UP) {
                toggleMultiplayerChatPanel();
                return true;
            }
            return true;
        });
        chatInput.setOnEditorActionListener((inputView, actionId, event) -> {
            boolean enterPressed = event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER &&
                    event.getAction() == KeyEvent.ACTION_DOWN;
            if (actionId == EditorInfo.IME_ACTION_SEND || actionId == EditorInfo.IME_ACTION_DONE || enterPressed) {
                sendCurrentChatMessage(chatInput);
                return true;
            }
            return false;
        });


        // Setup joystick movement
        setupJoystick(leftJoystick, leftJoystickKnob, true); // Left joystick

        setupLookAround(rightScreenArea);

        setupToggleButton(buttonToggle,buttonGroup);
        createHudEditBar();
        overlayView.post(() -> {
            captureHudDefaults();
            applySavedHudLayout();
        });

    }

    public void onMultiplayerChatMessage(String player, String message, int red, int green, int blue) {
        runOnUiThread(() -> {
            int color = Color.rgb(
                    Math.max(0, Math.min(255, red)),
                    Math.max(0, Math.min(255, green)),
                    Math.max(0, Math.min(255, blue)));
            String safePlayer = player == null || player.trim().isEmpty() ? "Player" : player.trim();
            String safeMessage = message == null ? "" : message.trim();
            if (safeMessage.isEmpty()) {
                return;
            }
            if (!pendingLocalPublicMessage.isEmpty() && safeMessage.equals(pendingLocalPublicMessage) &&
                    System.currentTimeMillis() - pendingLocalPublicMessageAtMs < 3000L) {
                safePlayer = getUiText("you");
                pendingLocalPublicMessage = "";
                pendingLocalPublicMessageAtMs = 0L;
            }
            playerColors.put(safePlayer, color);
            String target = "";
            if (safeMessage.startsWith("@")) {
                int space = safeMessage.indexOf(' ');
                if (space > 1) {
                    target = safeMessage.substring(1, space).trim();
                    safeMessage = safeMessage.substring(space + 1).trim();
                }
            }
            chatEntries.add(new ChatEntry(safePlayer, safeMessage, color, target));
            while (chatEntries.size() > 120) {
                chatEntries.remove(0);
            }
            if (chatPanelOpen) {
                renderChatHistory();
            }
        });
    }

    public void onMultiplayerPrivateChatMessage(String player, String message, String target, int red, int green, int blue) {
        runOnUiThread(() -> {
            int color = Color.rgb(
                    Math.max(0, Math.min(255, red)),
                    Math.max(0, Math.min(255, green)),
                    Math.max(0, Math.min(255, blue)));
            String safePlayer = player == null || player.trim().isEmpty() ? "Player" : player.trim();
            String safeMessage = message == null ? "" : message.trim();
            String safeTarget = target == null ? "" : target.trim();
            if (safeMessage.isEmpty()) {
                return;
            }
            playerColors.put(safePlayer, color);
            chatEntries.add(new ChatEntry(safePlayer, safeMessage, color, safeTarget));
            String peer = !safeTarget.isEmpty() ? safeTarget : safePlayer;
            if (!privateTargets.contains(peer)) {
                privateTargets.add(peer);
            }
            if (peer.equals(safePlayer) && !peer.equals(activePrivateTarget) && !unreadPrivateSenders.contains(peer)) {
                unreadPrivateSenders.add(peer);
            }
            setChatButtonPrivateAlert(!unreadPrivateSenders.isEmpty());
            while (chatEntries.size() > 120) {
                chatEntries.remove(0);
            }
            if (chatPanelOpen) {
                renderChatTabs();
                renderChatHistory();
            }
        });
    }

    private void addLocalPrivateChatToHistory(String target, String message) {
        String safeTarget = target == null ? "" : target.trim();
        String safeMessage = message == null ? "" : message.trim();
        if (safeTarget.isEmpty() || safeMessage.isEmpty()) {
            return;
        }
        int localColor = Color.rgb(100, 255, 100);
        chatEntries.add(new ChatEntry(getUiText("you"), safeMessage, localColor, safeTarget));
        if (!privateTargets.contains(safeTarget)) {
            privateTargets.add(safeTarget);
        }
        while (chatEntries.size() > 120) {
            chatEntries.remove(0);
        }
        if (chatPanelOpen) {
            renderChatTabs();
            renderChatHistory();
        }
    }

    public void setHudEditMode(boolean enabled) {
        runOnUiThread(() -> {
            hudEditMode = enabled;
            if (overlayView != null) {
                overlayView.setVisibility(View.VISIBLE);
                overlayView.bringToFront();
            }
            if (controllerButtonGroup != null) {
                controllerButtonGroup.setVisibility(View.VISIBLE);
            }
            if (hudEditBar != null) {
                updateHudEditBarTexts();
                hudEditBar.setVisibility(enabled ? View.VISIBLE : View.GONE);
                hudEditBar.bringToFront();
            }
            for (View view : hudEditableViews) {
                view.setVisibility(View.VISIBLE);
            }
            buttonToggle.setVisibility(enabled ? View.INVISIBLE : View.VISIBLE);
            if (chatPanel != null) {
                chatPanel.setVisibility(View.GONE);
            }
            chatPanelOpen = false;
            Toast.makeText(this,
                    enabled ? "HUD edit mode: drag controls or tap one for size/opacity." : "HUD edit mode off",
                    Toast.LENGTH_SHORT).show();
        });
    }

    private void createHudEditBar() {
        FrameLayout root = findViewById(android.R.id.content);
        float density = getResources().getDisplayMetrics().density;
        hudEditBar = new android.widget.LinearLayout(this);
        hudEditBar.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        hudEditBar.setGravity(Gravity.CENTER);
        hudEditBar.setVisibility(View.GONE);

        hudResetButton = makeBlueHudButton(getUiText("reset"));
        hudResetButton.setOnClickListener(v -> resetHudLayout());
        hudApplyButton = makeBlueHudButton(getUiText("apply"));
        hudApplyButton.setOnClickListener(v -> setHudEditMode(false));
        hudEditBar.addView(hudResetButton, new android.widget.LinearLayout.LayoutParams((int) (110 * density), (int) (42 * density)));
        android.widget.Space gap = new android.widget.Space(this);
        hudEditBar.addView(gap, new android.widget.LinearLayout.LayoutParams((int) (10 * density), 1));
        hudEditBar.addView(hudApplyButton, new android.widget.LinearLayout.LayoutParams((int) (110 * density), (int) (42 * density)));

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        params.topMargin = (int) (8 * density);
        root.addView(hudEditBar, params);
    }

    private void updateHudEditBarTexts() {
        if (hudResetButton != null) {
            hudResetButton.setText(getUiText("reset"));
        }
        if (hudApplyButton != null) {
            hudApplyButton.setText(getUiText("apply"));
        }
    }

    private Button makeBlueHudButton(String text) {
        Button button = new Button(this);
        button.setText(text);
        button.setTextColor(Color.WHITE);
        button.setTypeface(Typeface.DEFAULT_BOLD);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.argb(230, 24, 92, 150));
        bg.setStroke(2, Color.argb(240, 80, 180, 255));
        bg.setCornerRadius(8 * getResources().getDisplayMetrics().density);
        button.setBackground(bg);
        return button;
    }

    private String getUiText(String key) {
        int lang = getMenuLanguageIndex();
        if ("all".equals(key)) {
            if (lang == 1) return "Todos";
            if (lang == 2) return "Todos";
            return "All";
        }
        if ("private".equals(key)) {
            if (lang == 1) return "Privado";
            if (lang == 2) return "Privado";
            return "Private";
        }
        if ("apply".equals(key)) {
            if (lang == 0) return "Apply";
            if (lang == 2) return "Aplicar";
            return "Aplicar";
        }
        if ("reset".equals(key)) {
            if (lang == 1) return "Redefinir";
            if (lang == 2) return "Restablecer";
            return "Reset";
        }
        if ("size".equals(key)) {
            if (lang == 1) return "Tamanho";
            if (lang == 2) return "Tamaño";
            return "Size";
        }
        if ("opacity".equals(key)) {
            if (lang == 1) return "Opacidade";
            if (lang == 2) return "Opacidad";
            return "Opacity";
        }
        if ("done".equals(key)) {
            if (lang == 1) return "Concluído";
            if (lang == 2) return "Listo";
            return "Done";
        }
        if ("reset_this".equals(key)) {
            if (lang == 1) return "Redefinir este";
            if (lang == 2) return "Restablecer este";
            return "Reset this";
        }
        if ("joystick".equals(key)) {
            return "Joystick";
        }
        if ("hud_reset_toast".equals(key)) {
            if (lang == 1) return "Layout do HUD redefinido";
            if (lang == 2) return "Diseño del HUD restablecido";
            return "HUD layout reset";
        }
        if ("you".equals(key)) {
            if (lang == 0) return "You";
            if (lang == 2) return "Tú";
            return "Você";
        }
        return key;
    }

    private int getMenuLanguageIndex() {
        try {
            int nativeLanguage = getNativeMenuLanguage();
            if (nativeLanguage >= 0 && nativeLanguage <= 2) {
                return nativeLanguage;
            }
        } catch (UnsatisfiedLinkError ignored) {
        } catch (Exception ignored) {
        }
        String lang = Locale.getDefault().getLanguage();
        if ("pt".equals(lang)) return 1;
        if ("es".equals(lang)) return 2;
        return 0;
    }

    private void setChatButtonPrivateAlert(boolean alert) {
        buttonChat.setBackgroundTintList(alert
                ? ColorStateList.valueOf(Color.argb(90, 255, 0, 0))
                : null);
    }

    public void resetHudLayout() {
        runOnUiThread(() -> {
            SharedPreferences.Editor editor = preferences.edit();
            for (View view : hudEditableViews) {
                String key = getResources().getResourceEntryName(view.getId());
                editor.remove("hud." + key + ".x");
                editor.remove("hud." + key + ".y");
                editor.remove("hud." + key + ".scale");
                editor.remove("hud." + key + ".alpha");
            }
            editor.apply();
            for (View view : hudEditableViews) {
                Float defaultX = (Float) view.getTag(R.id.hud_default_x);
                Float defaultY = (Float) view.getTag(R.id.hud_default_y);
                if (defaultX != null && defaultY != null) {
                    view.setX(defaultX);
                    view.setY(defaultY);
                }
                applyHudScale(view, 1.0f);
                view.setAlpha(1.0f);
            }
            Toast.makeText(this, getUiText("hud_reset_toast"), Toast.LENGTH_SHORT).show();
        });
    }

    private void registerHudEditableViews() {
        hudEditableViews.clear();
        hudEditableViews.add(leftJoystick);
        hudEditableViews.add(buttonA);
        hudEditableViews.add(buttonB);
        hudEditableViews.add(buttonX);
        hudEditableViews.add(buttonY);
        hudEditableViews.add(buttonDpadUp);
        hudEditableViews.add(buttonDpadDown);
        hudEditableViews.add(buttonDpadLeft);
        hudEditableViews.add(buttonDpadRight);
        hudEditableViews.add(cDpadCross);
        hudEditableViews.add(buttonLB);
        hudEditableViews.add(buttonRB);
        hudEditableViews.add(buttonZ);
        hudEditableViews.add(buttonStart);
        hudEditableViews.add(buttonBack);
        hudEditableViews.add(buttonChat);

        cDpadViews.clear();
        cDpadViews.add(buttonDpadUp);
        cDpadViews.add(buttonDpadDown);
        cDpadViews.add(buttonDpadLeft);
        cDpadViews.add(buttonDpadRight);
        cDpadViews.add(cDpadCross);
        cDpadCross.setOnTouchListener((v, event) -> hudEditMode && handleHudEditTouch(v, event));
    }

    private void captureHudDefaults() {
        for (View view : hudEditableViews) {
            if (view.getTag(R.id.hud_default_x) == null) {
                view.setTag(R.id.hud_default_x, view.getX());
                view.setTag(R.id.hud_default_y, view.getY());
            }
        }
    }

    private void applySavedHudLayout() {
        ViewGroup parent = (ViewGroup) overlayView.findViewById(R.id.button_group);
        int parentWidth = Math.max(1, parent.getWidth());
        int parentHeight = Math.max(1, parent.getHeight());
        for (View view : hudEditableViews) {
            String key = getResources().getResourceEntryName(view.getId());
            if (preferences.contains("hud." + key + ".x")) {
                if (view == buttonBack || view == buttonChat) {
                    continue;
                }
                view.setX(preferences.getFloat("hud." + key + ".x", 0f) * parentWidth);
                view.setY(preferences.getFloat("hud." + key + ".y", 0f) * parentHeight);
            }
            applyHudScale(view, (view == buttonBack || view == buttonChat) ? 1.0f : preferences.getFloat("hud." + key + ".scale", 1.0f));
            view.setAlpha((view == buttonBack || view == buttonChat) ? 1.0f : preferences.getFloat("hud." + key + ".alpha", 1.0f));
        }
    }

    private boolean handleHudEditTouch(View view, MotionEvent event) {
        ViewGroup parent = (ViewGroup) view.getParent();
        if (parent == null) {
            return true;
        }
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                hudDragStartRawX = event.getRawX();
                hudDragStartRawY = event.getRawY();
                hudDragOffsetX = event.getRawX() - view.getX();
                hudDragOffsetY = event.getRawY() - view.getY();
                hudDragged = false;
                hudGroupDragStarts.clear();
                if (isCButtonView(view)) {
                    for (View item : cDpadViews) {
                        hudGroupDragStarts.put(item, new float[]{item.getX(), item.getY()});
                    }
                }
                view.bringToFront();
                return true;
            case MotionEvent.ACTION_MOVE:
                float slop = HUD_DRAG_CLICK_SLOP_DP * getResources().getDisplayMetrics().density;
                if (Math.abs(event.getRawX() - hudDragStartRawX) > slop ||
                        Math.abs(event.getRawY() - hudDragStartRawY) > slop) {
                    hudDragged = true;
                }
                if (isCButtonView(view) && !hudGroupDragStarts.isEmpty()) {
                    moveHudGroup(cDpadViews, event.getRawX() - hudDragStartRawX,
                            event.getRawY() - hudDragStartRawY, parent);
                } else {
                    float newX = event.getRawX() - hudDragOffsetX;
                    float newY = event.getRawY() - hudDragOffsetY;
                    newX = Math.max(0f, Math.min(newX, parent.getWidth() - view.getWidth()));
                    newY = Math.max(0f, Math.min(newY, parent.getHeight() - view.getHeight()));
                    view.setX(newX);
                    view.setY(newY);
                    saveHudViewLayout(view);
                }
                return true;
            case MotionEvent.ACTION_UP:
                saveHudViewLayout(view);
                if (!hudDragged) {
                    showHudControlOptions(view);
                }
                return true;
            case MotionEvent.ACTION_CANCEL:
                saveHudViewLayout(view);
                return true;
        }
        return true;
    }

    private boolean isCButtonView(View view) {
        return cDpadViews.contains(view);
    }

    private void moveHudGroup(List<View> views, float dx, float dy, ViewGroup parent) {
        for (View item : views) {
            float[] start = hudGroupDragStarts.get(item);
            if (start == null) {
                continue;
            }
            float newX = Math.max(0f, Math.min(start[0] + dx, parent.getWidth() - item.getWidth()));
            float newY = Math.max(0f, Math.min(start[1] + dy, parent.getHeight() - item.getHeight()));
            item.setX(newX);
            item.setY(newY);
            saveHudViewLayout(item);
        }
    }

    private void saveHudViewLayout(View view) {
        ViewGroup parent = (ViewGroup) view.getParent();
        if (parent == null || parent.getWidth() <= 0 || parent.getHeight() <= 0) {
            return;
        }
        String key = getResources().getResourceEntryName(view.getId());
        preferences.edit()
                .putFloat("hud." + key + ".x", view.getX() / parent.getWidth())
                .putFloat("hud." + key + ".y", view.getY() / parent.getHeight())
                .putFloat("hud." + key + ".scale", view.getScaleX())
                .putFloat("hud." + key + ".alpha", view.getAlpha())
                .apply();
    }

    private void showHudControlOptions(View view) {
        String key = getResources().getResourceEntryName(view.getId());
        android.widget.LinearLayout layout = new android.widget.LinearLayout(this);
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        int padding = (int) (20 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding / 2, padding, 0);

        TextView sizeLabel = new TextView(this);
        sizeLabel.setText(getUiText("size"));
        sizeLabel.setTextColor(Color.WHITE);
        SeekBar size = new SeekBar(this);
        size.setMax(120);
        size.setProgress(Math.round((view.getScaleX() - 0.6f) * 100f));

        TextView opacityLabel = new TextView(this);
        opacityLabel.setText(getUiText("opacity"));
        opacityLabel.setTextColor(Color.WHITE);
        SeekBar opacity = new SeekBar(this);
        opacity.setMax(90);
        opacity.setProgress(Math.round((view.getAlpha() - 0.1f) * 100f));

        layout.addView(sizeLabel);
        layout.addView(size);
        layout.addView(opacityLabel);
        layout.addView(opacity);

        size.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                applyHudScale(view, 0.6f + (progress / 100f));
                saveHudViewLayout(view);
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        opacity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                view.setAlpha(0.1f + (progress / 100f));
                saveHudViewLayout(view);
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        TextView title = new TextView(this);
        title.setText(getHudControlDisplayName(view));
        title.setGravity(Gravity.CENTER);
        title.setTextColor(Color.WHITE);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setTextSize(20);
        title.setPadding(0, padding, 0, padding / 2);

        new AlertDialog.Builder(this)
                .setCustomTitle(title)
                .setView(layout)
                .setNegativeButton(getUiText("reset_this"), (dialog, which) -> {
                    preferences.edit()
                            .remove("hud." + key + ".x")
                            .remove("hud." + key + ".y")
                            .remove("hud." + key + ".scale")
                            .remove("hud." + key + ".alpha")
                            .apply();
                    Float defaultX = (Float) view.getTag(R.id.hud_default_x);
                    Float defaultY = (Float) view.getTag(R.id.hud_default_y);
                    if (defaultX != null && defaultY != null) {
                        view.setX(defaultX);
                        view.setY(defaultY);
                    }
                    applyHudScale(view, 1.0f);
                    view.setAlpha(1.0f);
                })
                .setPositiveButton(getUiText("done"), null)
                .show();
    }

    private String getHudControlDisplayName(View view) {
        if (view == leftJoystick) return getUiText("joystick");
        if (view == buttonA) return "A";
        if (view == buttonB) return "B";
        if (view == buttonX) return "X";
        if (view == buttonY) return "Y";
        if (view == buttonLB) return "L";
        if (view == buttonRB) return "R";
        if (view == buttonZ) return "Z";
        if (view == buttonStart) return "Start";
        if (view == buttonBack) return "Back";
        if (view == buttonChat) return "Chat";
        if (isCButtonView(view)) return "C↑ C↓ C← C→";
        return getResources().getResourceEntryName(view.getId());
    }

    private void applyHudScale(View view, float scale) {
        scale = Math.max(0.6f, Math.min(1.8f, scale));
        view.setScaleX(scale);
        view.setScaleY(scale);
    }

    private void openMultiplayerChatKeyboard() {
        chatInput.setVisibility(View.VISIBLE);
        chatInput.requestFocus();
        chatInput.post(() -> {
            InputMethodManager keyboard = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            keyboard.showSoftInput(chatInput, InputMethodManager.SHOW_IMPLICIT);
        });
    }

    private void toggleMultiplayerChatPanel() {
        if (chatPanelOpen) {
            closeMultiplayerChatPanel();
        } else {
            openMultiplayerChatPanel();
        }
    }

    private void openMultiplayerChatPanel() {
        if (chatPanel == null) {
            createMultiplayerChatPanel();
        }
        chatPanelOpen = true;
        chatPanel.setVisibility(View.VISIBLE);
        updateControlsForChatPanel(true);
        renderChatTabs();
        renderChatHistory();
    }

    private void closeMultiplayerChatPanel() {
        chatPanelOpen = false;
        if (chatPanel != null) {
            chatPanel.setVisibility(View.GONE);
        }
        if (privateTabsPanel != null) {
            privateTabsPanel.setVisibility(View.GONE);
        }
        updateControlsForChatPanel(false);
        if (chatPanelInput != null) {
            chatPanelInput.clearFocus();
            InputMethodManager keyboard = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            keyboard.hideSoftInputFromWindow(chatPanelInput.getWindowToken(), 0);
        }
    }

    private void updateControlsForChatPanel(boolean open) {
        for (View view : hudEditableViews) {
            if (view == buttonBack || view == buttonChat) {
                view.setVisibility(View.VISIBLE);
            } else {
                view.setVisibility(open ? View.INVISIBLE : View.VISIBLE);
            }
        }
        if (rightScreenArea != null) {
            rightScreenArea.setVisibility(open ? View.GONE : View.VISIBLE);
        }
        buttonToggle.setVisibility(open ? View.INVISIBLE : View.VISIBLE);
    }

    private void createMultiplayerChatPanel() {
        FrameLayout root = findViewById(android.R.id.content);
        float density = getResources().getDisplayMetrics().density;
        int panelWidth = Math.max((int) (360 * density), (int) (getResources().getDisplayMetrics().widthPixels * 0.52f));
        int panelHeight = Math.max((int) (210 * density), (int) (getResources().getDisplayMetrics().heightPixels * 0.38f));
        panelWidth = Math.min(panelWidth, (int) (getResources().getDisplayMetrics().widthPixels * 0.72f));
        panelHeight = Math.min(panelHeight, (int) (getResources().getDisplayMetrics().heightPixels * 0.58f));

        chatPanel = new FrameLayout(this);
        GradientDrawable panelBg = new GradientDrawable();
        panelBg.setColor(Color.argb(188, 9, 16, 24));
        panelBg.setStroke((int) (1 * density), Color.argb(220, 55, 90, 120));
        panelBg.setCornerRadius(6 * density);
        chatPanel.setBackground(panelBg);
        chatPanel.setClickable(true);
        chatPanel.setVisibility(View.GONE);

        android.widget.LinearLayout content = new android.widget.LinearLayout(this);
        content.setOrientation(android.widget.LinearLayout.VERTICAL);
        content.setPadding((int) (6 * density), 0, (int) (6 * density), (int) (6 * density));

        chatTabsLayout = new android.widget.LinearLayout(this);
        chatTabsLayout.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        chatTabsLayout.setGravity(Gravity.CENTER_VERTICAL);
        content.addView(chatTabsLayout, new android.widget.LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, (int) (36 * density)));

        android.widget.LinearLayout body = new android.widget.LinearLayout(this);
        body.setOrientation(android.widget.LinearLayout.HORIZONTAL);

        chatHistoryScroll = new ScrollView(this);
        chatHistoryScroll.setVerticalScrollBarEnabled(true);
        chatHistoryScroll.setScrollbarFadingEnabled(false);
        chatHistoryLayout = new android.widget.LinearLayout(this);
        chatHistoryLayout.setOrientation(android.widget.LinearLayout.VERTICAL);
        chatHistoryScroll.addView(chatHistoryLayout, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        body.addView(chatHistoryScroll, new android.widget.LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.MATCH_PARENT, 1f));

        content.addView(body, new android.widget.LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        android.widget.LinearLayout inputRow = new android.widget.LinearLayout(this);
        inputRow.setOrientation(android.widget.LinearLayout.HORIZONTAL);
        inputRow.setGravity(Gravity.CENTER_VERTICAL);
        chatPanelInput = new EditText(this);
        chatPanelInput.setSingleLine(true);
        chatPanelInput.setFilters(new InputFilter[]{new InputFilter.LengthFilter(95)});
        chatPanelInput.setTextColor(Color.WHITE);
        chatPanelInput.setHintTextColor(Color.argb(150, 255, 255, 255));
        chatPanelInput.setHint("Type a message");
        chatPanelInput.setTextSize(13);
        chatPanelInput.setImeOptions(EditorInfo.IME_ACTION_SEND);
        chatPanelInput.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
        chatPanelInput.setBackgroundColor(Color.argb(160, 18, 18, 18));
        chatPanelInput.setOnEditorActionListener((inputView, actionId, event) -> {
            boolean enterPressed = event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER &&
                    event.getAction() == KeyEvent.ACTION_DOWN;
            if (actionId == EditorInfo.IME_ACTION_SEND || actionId == EditorInfo.IME_ACTION_DONE || enterPressed) {
                sendCurrentChatMessage(chatPanelInput);
                return true;
            }
            return false;
        });
        Button send = new Button(this);
        send.setText("➤");
        send.setTextColor(Color.LTGRAY);
        send.setTextSize(18);
        send.setBackgroundColor(Color.argb(120, 35, 35, 35));
        send.setOnClickListener(v -> sendCurrentChatMessage(chatPanelInput));
        inputRow.addView(chatPanelInput, new android.widget.LinearLayout.LayoutParams(0, (int) (42 * density), 1f));
        inputRow.addView(send, new android.widget.LinearLayout.LayoutParams((int) (46 * density), (int) (42 * density)));
        content.addView(inputRow, new android.widget.LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, (int) (46 * density)));

        chatPanel.addView(content, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(panelWidth, panelHeight);
        params.leftMargin = (int) (6 * density);
        params.topMargin = (int) (4 * density);
        root.addView(chatPanel, params);

        privateTabsPanel = new FrameLayout(this);
        privateTabsPanel.setVisibility(View.GONE);
        GradientDrawable privateBg = new GradientDrawable();
        privateBg.setColor(Color.argb(180, 9, 16, 24));
        privateBg.setStroke((int) (1 * density), Color.argb(220, 55, 90, 120));
        privateBg.setCornerRadius(6 * density);
        privateTabsPanel.setBackground(privateBg);
        privateTabsScroll = new ScrollView(this);
        privateTabsScroll.setVerticalScrollBarEnabled(true);
        privateTabsScroll.setScrollbarFadingEnabled(false);
        privateTabsLayout = new android.widget.LinearLayout(this);
        privateTabsLayout.setOrientation(android.widget.LinearLayout.VERTICAL);
        privateTabsScroll.addView(privateTabsLayout, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        privateTabsPanel.addView(privateTabsScroll, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        FrameLayout.LayoutParams privateParams = new FrameLayout.LayoutParams(
                (int) (126 * density), panelHeight);
        privateParams.leftMargin = params.leftMargin + panelWidth + (int) (6 * density);
        privateParams.topMargin = params.topMargin;
        root.addView(privateTabsPanel, privateParams);
    }

    private TextView makeChatTab(String text, boolean active) {
        TextView tab = new TextView(this);
        tab.setText(text);
        tab.setTextColor(active ? Color.WHITE : Color.LTGRAY);
        tab.setTypeface(Typeface.DEFAULT_BOLD);
        tab.setGravity(Gravity.CENTER);
        tab.setTextSize(13);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(active ? Color.argb(210, 24, 42, 58) : Color.argb(160, 23, 23, 23));
        bg.setStroke(1, Color.argb(160, 80, 90, 100));
        tab.setBackground(bg);
        return tab;
    }

    private void renderChatTabs() {
        if (chatTabsLayout == null) {
            return;
        }
        chatTabsLayout.removeAllViews();
        TextView general = makeChatTab(getUiText("all"), activePrivateTarget.isEmpty());
        general.setOnClickListener(v -> {
            activePrivateTarget = "";
            renderChatTabs();
            renderChatHistory();
        });
        TextView particular = makeChatTab(getUiText("private"), !activePrivateTarget.isEmpty());
        particular.setOnClickListener(v -> {
            if (!privateTargets.isEmpty()) {
                activePrivateTarget = privateTargets.get(0);
            }
            renderChatTabs();
            renderChatHistory();
        });
        chatTabsLayout.addView(general, new android.widget.LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f));
        chatTabsLayout.addView(particular, new android.widget.LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f));

        purgeExpiredChatEntries();
        privateTabsLayout.removeAllViews();
        privateTabsPanel.setVisibility(!activePrivateTarget.isEmpty() && !privateTargets.isEmpty() ? View.VISIBLE : View.GONE);
        for (String target : new ArrayList<>(privateTargets)) {
            String label = target.length() > 20 ? target.substring(0, 20) : target;
            android.widget.LinearLayout row = new android.widget.LinearLayout(this);
            row.setOrientation(android.widget.LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            GradientDrawable rowBg = new GradientDrawable();
            rowBg.setColor(unreadPrivateSenders.contains(target)
                    ? Color.argb(190, 150, 20, 20)
                    : (target.equals(activePrivateTarget) ? Color.argb(210, 24, 42, 58) : Color.argb(160, 23, 23, 23)));
            rowBg.setStroke(1, Color.argb(160, 80, 90, 100));
            row.setBackground(rowBg);

            TextView tab = new TextView(this);
            tab.setText(label);
            tab.setTextColor(playerColors.containsKey(target) ? playerColors.get(target) : Color.LTGRAY);
            tab.setTypeface(Typeface.DEFAULT_BOLD);
            tab.setTextSize(12);
            tab.setGravity(Gravity.CENTER_VERTICAL);
            tab.setPadding(6, 0, 4, 0);
            tab.setOnClickListener(v -> {
                activePrivateTarget = target;
                unreadPrivateSenders.remove(target);
                setChatButtonPrivateAlert(!unreadPrivateSenders.isEmpty());
                renderChatTabs();
                renderChatHistory();
            });
            Button close = new Button(this);
            close.setText("×");
            close.setTextColor(Color.WHITE);
            close.setTextSize(12);
            close.setPadding(0, 0, 0, 0);
            GradientDrawable closeBg = new GradientDrawable();
            closeBg.setColor(Color.argb(210, 170, 20, 20));
            closeBg.setCornerRadius(4 * getResources().getDisplayMetrics().density);
            close.setBackground(closeBg);
            close.setOnClickListener(v -> {
                deletePrivateConversation(target);
            });
            row.addView(tab, new android.widget.LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f));
            row.addView(close, new android.widget.LinearLayout.LayoutParams(
                    (int) (30 * getResources().getDisplayMetrics().density),
                    ViewGroup.LayoutParams.MATCH_PARENT));
            privateTabsLayout.addView(row, new android.widget.LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    (int) (34 * getResources().getDisplayMetrics().density)));
        }
    }

    private void renderChatHistory() {
        if (chatHistoryLayout == null) {
            return;
        }
        purgeExpiredChatEntries();
        chatHistoryLayout.removeAllViews();
        for (ChatEntry entry : chatEntries) {
            if (activePrivateTarget.isEmpty() && !entry.target.isEmpty()) {
                continue;
            }
            if (!activePrivateTarget.isEmpty()) {
                boolean privateWithSelected = activePrivateTarget.equals(entry.target) ||
                        (activePrivateTarget.equals(entry.player) && !entry.target.isEmpty());
                if (!privateWithSelected) {
                    continue;
                }
            }
            TextView line = new TextView(this);
            line.setTextSize(11);
            line.setTextColor(Color.WHITE);
            line.setPadding(4, 3, 4, 3);
            boolean isPrivate = !entry.target.isEmpty();
            String privatePrefix = "";
            String prefix = entry.player + ": ";
            SpannableString text = new SpannableString(privatePrefix + prefix + entry.message);
            if (isPrivate) {
                text.setSpan(new android.text.style.ForegroundColorSpan(Color.RED), 0, privatePrefix.length(),
                        Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
            text.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View widget) {
                    openPrivateConversation(entry.player);
                }

                @Override
                public void updateDrawState(TextPaint ds) {
                    ds.setColor(entry.color);
                    ds.setUnderlineText(false);
                    ds.setFakeBoldText(true);
                }
            }, privatePrefix.length(), privatePrefix.length() + Math.max(0, prefix.length() - 2),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            line.setText(text);
            line.setMovementMethod(LinkMovementMethod.getInstance());
            line.setHighlightColor(Color.TRANSPARENT);
            line.setOnLongClickListener(v -> {
                chatEntries.remove(entry);
                renderChatHistory();
                return true;
            });
            chatHistoryLayout.addView(line, new android.widget.LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        chatHistoryScroll.post(() -> chatHistoryScroll.fullScroll(View.FOCUS_DOWN));
    }

    private void purgeExpiredChatEntries() {
        long cutoff = System.currentTimeMillis() - 5L * 60L * 1000L;
        chatEntries.removeIf(entry -> entry.createdAtMs < cutoff);
    }

    private void openPrivateConversation(String player) {
        if (player == null || player.trim().isEmpty()) {
            return;
        }
        if (!privateTargets.contains(player)) {
            privateTargets.add(player);
        }
        activePrivateTarget = player;
        renderChatTabs();
        renderChatHistory();
    }

    private void deletePrivateConversation(String target) {
        privateTargets.remove(target);
        unreadPrivateSenders.remove(target);
        setChatButtonPrivateAlert(!unreadPrivateSenders.isEmpty());
        chatEntries.removeIf(entry -> target.equals(entry.player) || target.equals(entry.target));
        if (target.equals(activePrivateTarget)) {
            activePrivateTarget = "";
        }
        renderChatTabs();
        renderChatHistory();
    }

    private void sendCurrentChatMessage(EditText input) {
        String message = input.getText().toString().trim();
        if (!message.isEmpty()) {
            String privateTarget = activePrivateTarget;
            String privateMessage = message;
            if (privateTarget.isEmpty() && message.startsWith("@")) {
                int space = message.indexOf(' ');
                if (space > 1) {
                    privateTarget = message.substring(1, space).trim();
                    privateMessage = message.substring(space + 1).trim();
                }
            }
            String outgoing = activePrivateTarget.isEmpty() ? message : "@" + activePrivateTarget + " " + message;
            if (!privateTarget.isEmpty() && !privateMessage.isEmpty()) {
                addLocalPrivateChatToHistory(privateTarget, privateMessage);
            } else {
                pendingLocalPublicMessage = message;
                pendingLocalPublicMessageAtMs = System.currentTimeMillis();
            }
            sendMultiplayerChat(outgoing);
        }
        input.setText("");
        input.clearFocus();
        InputMethodManager keyboard = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        keyboard.hideSoftInputFromWindow(input.getWindowToken(), 0);
        if (input == chatInput) {
            chatInput.setVisibility(View.GONE);
        }
    }

    private void setupToggleButton(Button button, ViewGroup uiGroup){
        boolean isHidden = preferences.getBoolean("controlsVisible", false); // Default to 'false' (visible)
        uiGroup.setVisibility(isHidden ? View.INVISIBLE : View.VISIBLE); // Set the initial visibility based on the saved state
        //if(isHidden){
        //    detachController();
        //}
        button.setOnClickListener(new View.OnClickListener() {
            boolean isHidden = preferences.getBoolean("controlsVisible", false);
            @Override
            public void onClick(View v) {
                if (isHidden) {
                    uiGroup.setVisibility(View.VISIBLE); // Show UI elements
                    //attachController();
                } else {
                    uiGroup.setVisibility(View.INVISIBLE); // Hide UI elements
                    //detachController();
                }
                preferences.edit().putBoolean("controlsVisible", !isHidden).apply();
                isHidden = !isHidden; // Toggle state
            }
        });
    }

    // Function to set a touch listener for each button
    private void addTouchListener(Button button, int buttonNum) {
        button.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (hudEditMode) {
                    return handleHudEditTouch(v, event);
                }
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        setButton(buttonNum, true);
                        button.setPressed(true);
                        return true;
                    case MotionEvent.ACTION_UP:
                        setButton(buttonNum, false);
                        button.setPressed(false);
                        return true;
                    case MotionEvent.ACTION_CANCEL:
                        setButton(buttonNum, false);
                        return true;
                }
                return false;
            }
        });
    }

    private void setupCButtons(Button button, int buttonNum, int direction) {
        button.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                if (hudEditMode) {
                    return handleHudEditTouch(v, event);
                }
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        setAxis(buttonNum, direction<0 ? Short.MAX_VALUE : Short.MIN_VALUE);
                        button.setPressed(true);
                        return true;
                    case MotionEvent.ACTION_UP:
                        setAxis(buttonNum, (short) 0);
                        button.setPressed(false);
                        return true;
                    case MotionEvent.ACTION_CANCEL:
                        setAxis(buttonNum, (short) 0);
                        return true;
                }
                return false;
            }
        });
    }

    boolean TouchAreaEnabled = true;

    void DisableTouchArea(){
        TouchAreaEnabled = false;
    }
    void EnableTouchArea(){
        TouchAreaEnabled = true;
    }

    private void setupLookAround(FrameLayout rightScreenArea) {
        rightScreenArea.setOnTouchListener(new View.OnTouchListener() {
            private float lastX = 0;
            private float lastY = 0;
            private boolean isTouching = false;

            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        // Start tracking the finger's position
                        lastX = event.getX();
                        lastY = event.getY();
                        isTouching = true;
                        break;

                    case MotionEvent.ACTION_MOVE:
                        if (isTouching) {
                            // Calculate the change in position (delta)
                            float deltaX = event.getX() - lastX;
                            float deltaY = event.getY() - lastY;

                            // Update the last position
                            lastX = event.getX();
                            lastY = event.getY();

                            // Increase sensitivity by using a larger multiplier
                            // Adjust these multipliers to suit your needs
                            float sensitivityMultiplier = 15; // Higher value for more sensitivity
                            float rx = (deltaX * sensitivityMultiplier);
                            float ry = (deltaY * sensitivityMultiplier);

                            // Send the mapped values to the joystick axes
                            setCameraState(0, rx); // Right stick X axis
                            setCameraState(1, ry); // Right stick Y axis
                        }
                        break;

                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        // Stop tracking the finger's position and reset joystick input
                        isTouching = false;
                        setCameraState(0, 0.0f); // Reset right stick X axis
                        setCameraState(1, 0.0f); // Reset right stick Y axis
                        break;
                }
                return TouchAreaEnabled; // Event full handled
            }
        });
    }





    // Function to set joystick movement with reset to center when not touched
    private void setupJoystick(FrameLayout joystickLayout, ImageView joystickKnob, boolean isLeft) {
        joystickLayout.post(() -> {
            joystickLayout.setOnTouchListener(new View.OnTouchListener() {
                @Override
                public boolean onTouch(View v, MotionEvent event) {
                    if (hudEditMode) {
                        return handleHudEditTouch(v, event);
                    }
                    switch (event.getActionMasked()) {
                        case MotionEvent.ACTION_DOWN:
                        case MotionEvent.ACTION_MOVE:
                            // Recalculate from the current measured size. Android can resize the surface after
                            // startup because of navigation bars, cutouts or resolution changes.
                            float joystickCenterX = joystickLayout.getWidth() / 2f;
                            float joystickCenterY = joystickLayout.getHeight() / 2f;
                            float deltaX = event.getX() - joystickCenterX;
                            float deltaY = event.getY() - joystickCenterY;

                            float maxRadius = Math.max(1f,
                                    Math.min(joystickLayout.getWidth(), joystickLayout.getHeight()) / 2f -
                                            Math.max(joystickKnob.getWidth(), joystickKnob.getHeight()) / 2f);
                            float distance = (float) Math.sqrt(deltaX * deltaX + deltaY * deltaY);
                            if (distance > maxRadius) {
                                float scale = maxRadius / distance;
                                deltaX *= scale;
                                deltaY *= scale;
                            }

                            // The knob is centered by layout_gravity; translation remains correct under every density.
                            joystickKnob.setTranslationX(deltaX);
                            joystickKnob.setTranslationY(deltaY);

                            short x = (short) Math.round(deltaX / maxRadius * Short.MAX_VALUE);
                            short y = (short) Math.round(deltaY / maxRadius * Short.MAX_VALUE);
                            setAxis(isLeft ? ControllerButtons.AXIS_LX : ControllerButtons.AXIS_RX, x);
                            setAxis(isLeft ? ControllerButtons.AXIS_LY : ControllerButtons.AXIS_RY, y);
                            break;

                        case MotionEvent.ACTION_UP:
                        case MotionEvent.ACTION_CANCEL:
                            joystickKnob.setTranslationX(0f);
                            joystickKnob.setTranslationY(0f);
                            setAxis(isLeft ? ControllerButtons.AXIS_LX : ControllerButtons.AXIS_RX, (short) 0);
                            setAxis(isLeft ? ControllerButtons.AXIS_LY : ControllerButtons.AXIS_RY, (short) 0);
                            break;
                    }
                    return true;
                }
            });
        });


    }


}

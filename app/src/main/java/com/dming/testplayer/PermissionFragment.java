package com.dming.testplayer;

import android.Manifest;
import android.annotation.TargetApi;
import android.content.pm.PackageManager;
import android.os.Build;
import android.widget.Toast;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

public class PermissionFragment extends Fragment {
    public OnRequestPermissions mOnRequestPermissions;
    private final int REQUEST_STORAGE = 123;
    private final static String TAG = "PermissionFragment";

    public void setPermissionListener(OnRequestPermissions onRequestPermissions) {
        this.mOnRequestPermissions = onRequestPermissions;
    }

    public static void checkPermission(final FragmentActivity fragmentActivity, final Runnable runnable) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (fragmentActivity.checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                FragmentManager fragmentManager = fragmentActivity.getSupportFragmentManager();
                PermissionFragment btFragment = (PermissionFragment) fragmentManager.findFragmentByTag(TAG);
                if (btFragment == null) {
                    btFragment = new PermissionFragment();
                    fragmentManager
                            .beginTransaction()
                            .add(btFragment, TAG)
                            .commitNow();
                    btFragment.setPermissionListener(new PermissionFragment.OnRequestPermissions() {
                        @Override
                        public void result(boolean granted) {
                            if (granted) {
                                runnable.run();
                            } else {
                                Toast.makeText(fragmentActivity,"需要获取访问SD卡权限", Toast.LENGTH_SHORT).show();
                                fragmentActivity.finish();
                            }
                        }
                    });
                }
                btFragment.requestBTPermission();
            } else {
                runnable.run();
            }
        } else {
            runnable.run();
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    public void requestBTPermission() {
        requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, REQUEST_STORAGE);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == REQUEST_STORAGE) {
            boolean granted = false;
            for (int i = 0, size = permissions.length; i < size; i++) {
                granted = grantResults[i] == PackageManager.PERMISSION_GRANTED;
                if (!granted) {
                    break;
                }
            }
            if (this.mOnRequestPermissions != null) {
                this.mOnRequestPermissions.result(granted);
            }
        }
    }

    public interface OnRequestPermissions {
        void result(boolean granted);
    }
}

package com.dming.testplayer;

/**
 *
 * Created by DMing at 2017/12/20 0020
 */

public class DUtils {

//    /**
//     //透明状态栏
//     getWindow().addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
//     //透明导航栏
//     getWindow().addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
//
//     * 获取状态栏高度
//     * @param context
//     * @return
//     */
//    public static int getStatusBarHeight(Context context) {
//        int result = 0;
//        int resourceId = context.getResources().getIdentifier("status_bar_height", "dimen",
//                "android");
//        if (resourceId > 0) {
//            result = context.getResources().getDimensionPixelSize(resourceId);
//        }
//        return result;
//    }
//
//    public static boolean isNavigationBarShow(Activity activity){
//        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
//            Display display = activity.getWindowManager().getDefaultDisplay();
//            Point size = new Point();
//            Point realSize = new Point();
//            display.getSize(size);
//            display.getRealSize(realSize);
//            return realSize.y!=size.y;
//        }else {
//            boolean menu = ViewConfiguration.get(activity).hasPermanentMenuKey();
//            boolean back = KeyCharacterMap.deviceHasKey(KeyEvent.KEYCODE_BACK);
//            if(menu || back) {
//                return false;
//            }else {
//                return true;
//            }
//        }
//    }
//
//    public static int getNavigationBarHeight(Activity activity) {
//        if (!isNavigationBarShow(activity)){
//            return 0;
//        }
//        Resources resources = activity.getResources();
//        int resourceId = resources.getIdentifier("navigation_bar_height",
//                "dimen", "android");
//        //获取NavigationBar的高度
//        int height = resources.getDimensionPixelSize(resourceId);
//        return height;
//    }

    // a integer to xx:xx:xx
    public static String secToTime(long time) {
        String timeStr = null;
        int hour = 0;
        int minute = 0;
        int second = 0;
        if (time <= 0)
            return "00:00";
        else {
            minute = (int) (time / 60);
            if (minute < 60) {
                second = (int) (time % 60);
                timeStr = unitFormat(minute) + ":" + unitFormat(second);
//                DLog.i("timeStr: "+timeStr);
            } else {
                hour = minute / 60;
                if (hour > 99)
                    return "99:59:59";
                minute = minute % 60;
                second = (int) (time - hour * 3600 - minute * 60);
                timeStr = unitFormat(hour) + ":" + unitFormat(minute) + ":" + unitFormat(second);
            }
        }
        return timeStr;
    }

    public static String unitFormat(int i) {
        String retStr = null;
        if (i >= 0 && i < 10)
            retStr = "0" + Integer.toString(i);
        else
            retStr = "" + i;
        return retStr;
    }

}

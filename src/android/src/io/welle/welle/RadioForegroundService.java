package io.welle.welle;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;

public class RadioForegroundService extends Service {

    private static final String CHANNEL_ID = "WelleRadioChannel";

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Build the notification that signals to Android we are active
        // This is what the user sees in the notification shade
        Notification notification = new Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("welle.io")
                .setContentText("Radio playback active")
                .setSmallIcon(android.R.drawable.ic_media_play)
                .build();

        // Start the service in the foreground immediately
        startForeground(1, notification);

        return START_STICKY; // If killed by system, this tells it to restart
    }

    private void createNotificationChannel() {
        // Required for Android 8.0 (Oreo) and above
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID, 
                    "Radio Service", 
                    NotificationManager.IMPORTANCE_LOW
            );
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}


package edu.tongji.minididi;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.view.WindowManager;
import android.widget.*;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.*;

/** Foreground-only data capture. Keep this screen open during the live demo. */
public final class MainActivity extends Activity implements SensorEventListener,LocationListener {
    private SensorManager sensorManager;
    private LocationManager locationManager;
    private final PoseEstimator estimator=new PoseEstimator();
    private final Object sampleLock=new Object();
    private final float[] accel=new float[3],linear=new float[3],gyro=new float[3],magnetic=new float[3],gravity=new float[3];
    private final float[] rotation={1,0,0,0,1,0,0,0,1},orientation=new float[3];
    private final float[] fallbackRotation=new float[9];
    private boolean hasRotation=false,hasLinear=false;
    private volatile boolean enabled=false;
    private volatile boolean active=false;
    private EditText address;
    private TextView status,readings,availability;
    private Button connect;
    private volatile String serverUrl="";
    private volatile String session="";
    private volatile long seq=0;
    private volatile Location lastLocation;
    private ScheduledExecutorService network;

    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        sensorManager=(SensorManager)getSystemService(SENSOR_SERVICE);locationManager=(LocationManager)getSystemService(LOCATION_SERVICE);
        hasRotation=sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)!=null;
        hasLinear=sensorManager.getDefaultSensor(Sensor.TYPE_LINEAR_ACCELERATION)!=null;
        LinearLayout layout=new LinearLayout(this);layout.setOrientation(LinearLayout.VERTICAL);layout.setPadding(48,70,48,36);layout.setBackgroundColor(Color.rgb(244,247,239));
        TextView eyebrow=text("MINIDIDI  /  PHONE TWIN",12);eyebrow.setTextColor(Color.rgb(112,142,99));layout.addView(eyebrow);
        TextView title=text("手机数字孪生",30);layout.addView(title);
        layout.addView(text("以手机运动估计位姿，实时同步至数字世界。\n请保持此页面打开，电脑和手机连接同一 Wi-Fi。",14));
        address=new EditText(this);address.setSingleLine(true);address.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_VARIATION_URI);address.setHint("http://电脑局域网IP:8080");address.setText(getPreferences(MODE_PRIVATE).getString("server","http://192.168.1.10:8080"));layout.addView(address);
        connect=new Button(this);connect.setText("连接并开始同步");layout.addView(connect);
        status=text("未连接",15);layout.addView(status);readings=text("E 0.00 m  ·  N 0.00 m",19);layout.addView(readings);
        availability=text(sensorDescription(),12);layout.addView(availability);
        layout.addView(text("定位方法：线性加速度 → ENU 旋转 → 积分，静止时零速修正，室外用 GNSS 缓慢校正。短时演示仍可能漂移，当前实现不等同于 SLAM。",12));
        Button calibrate=new Button(this);calibrate.setText("起始位置归零（演示前使用）");calibrate.setOnClickListener(v->{estimator.reset();status.setText("已将当前位置设为原点");});layout.addView(calibrate);
        ScrollView scroll=new ScrollView(this);scroll.addView(layout);setContentView(scroll);
        connect.setOnClickListener(v->{
            if(enabled){enabled=false;connect.setText("连接并开始同步");status.setText("已暂停发送");return;}
            String raw=address.getText().toString().trim();while(raw.endsWith("/"))raw=raw.substring(0,raw.length()-1);
            try{URI uri=URI.create(raw);if(!"http".equalsIgnoreCase(uri.getScheme())||uri.getHost()==null||uri.getUserInfo()!=null||uri.getRawQuery()!=null||!(uri.getPath().isEmpty()||uri.getPath().equals("/")))throw new IllegalArgumentException();}
            catch(Exception ex){status.setText("请输入 http://电脑IP:8080 格式的地址");return;}
            serverUrl=raw;session="";seq=0;enabled=true;getPreferences(MODE_PRIVATE).edit().putString("server",serverUrl).apply();connect.setText("停止同步");status.setText("正在连接…");
            requestLocationPermission();
        });
        network=Executors.newSingleThreadScheduledExecutor();network.scheduleWithFixedDelay(this::sendSample,0,100,TimeUnit.MILLISECONDS);
    }
    private TextView text(String content,int size){TextView t=new TextView(this);t.setText(content);t.setTextSize(size);t.setTextColor(Color.rgb(53,78,55));t.setPadding(0,14,0,18);t.setLineSpacing(4,1);return t;}
    private String sensorDescription(){StringBuilder b=new StringBuilder("传感器状态\n");int[] types={Sensor.TYPE_ACCELEROMETER,Sensor.TYPE_LINEAR_ACCELERATION,Sensor.TYPE_GYROSCOPE,Sensor.TYPE_MAGNETIC_FIELD,Sensor.TYPE_ROTATION_VECTOR};String[] names={"加速度计","线性加速度","陀螺仪","地磁","旋转向量"};for(int i=0;i<types.length;i++)b.append(names[i]).append(": ").append(sensorManager.getDefaultSensor(types[i])!=null?"可用":"缺失").append("  ");return b.toString();}
    @Override protected void onResume(){super.onResume();active=true;int[] types={Sensor.TYPE_ACCELEROMETER,Sensor.TYPE_LINEAR_ACCELERATION,Sensor.TYPE_GYROSCOPE,Sensor.TYPE_MAGNETIC_FIELD,Sensor.TYPE_ROTATION_VECTOR};for(int type:types){Sensor sensor=sensorManager.getDefaultSensor(type);if(sensor!=null)sensorManager.registerListener(this,sensor,SensorManager.SENSOR_DELAY_GAME);}startLocation();}
    @Override protected void onPause(){active=false;sensorManager.unregisterListener(this);try{locationManager.removeUpdates(this);}catch(SecurityException ignored){}super.onPause();}
    @Override protected void onDestroy(){enabled=false;if(network!=null)network.shutdownNow();super.onDestroy();}
    private void requestLocationPermission(){if(checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)!=PackageManager.PERMISSION_GRANTED)requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION,Manifest.permission.ACCESS_COARSE_LOCATION},42);else startLocation();}
    @Override public void onRequestPermissionsResult(int code,String[] permissions,int[] results){super.onRequestPermissionsResult(code,permissions,results);if(code==42)startLocation();}
    private void startLocation(){try{if(checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED&&locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER))locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER,1000,0,this);}catch(SecurityException|IllegalArgumentException ignored){}}
    @Override public void onLocationChanged(Location location){lastLocation=location;estimator.correctGps(location.getLatitude(),location.getLongitude(),location.getAccuracy());}
    @Override public void onProviderEnabled(String provider){}
    @Override public void onProviderDisabled(String provider){}
    @Override public void onStatusChanged(String provider,int status,Bundle extras){}
    @Override public void onAccuracyChanged(Sensor sensor,int accuracy){}
    @Override public void onSensorChanged(SensorEvent event){
        synchronized(sampleLock){
            int type=event.sensor.getType();
            if(type==Sensor.TYPE_ROTATION_VECTOR){SensorManager.getRotationMatrixFromVector(rotation,event.values);SensorManager.getOrientation(rotation,orientation);return;}
            if(type==Sensor.TYPE_GYROSCOPE){System.arraycopy(event.values,0,gyro,0,3);return;}
            if(type==Sensor.TYPE_MAGNETIC_FIELD){System.arraycopy(event.values,0,magnetic,0,3);return;}
            if(type==Sensor.TYPE_ACCELEROMETER){
                System.arraycopy(event.values,0,accel,0,3);
                for(int i=0;i<3;i++)gravity[i]=.8f*gravity[i]+.2f*accel[i];
                if(!hasRotation&&SensorManager.getRotationMatrix(fallbackRotation,null,gravity,magnetic)){System.arraycopy(fallbackRotation,0,rotation,0,9);SensorManager.getOrientation(rotation,orientation);}
                if(hasLinear)return;for(int i=0;i<3;i++)linear[i]=accel[i]-gravity[i];
            }else if(type==Sensor.TYPE_LINEAR_ACCELERATION)System.arraycopy(event.values,0,linear,0,3);else return;
            estimator.update(linear,rotation,orientation[0],event.timestamp);
        }
    }
    private static JSONArray array(float[] values){JSONArray a=new JSONArray();for(float v:values)a.put((double)v);return a;}
    private JSONObject post(String path,JSONObject body)throws Exception{
        HttpURLConnection connection=(HttpURLConnection)new URL(serverUrl+path).openConnection();connection.setConnectTimeout(2500);connection.setReadTimeout(2500);connection.setRequestMethod("POST");connection.setRequestProperty("Content-Type","application/json; charset=utf-8");connection.setDoOutput(true);
        try{try(OutputStream out=connection.getOutputStream()){out.write(body.toString().getBytes(StandardCharsets.UTF_8));}int code=connection.getResponseCode();if(code==401){session="";throw new IOException("服务端已重置，正在重新注册");}if(code<200||code>=300)throw new IOException("HTTP "+code);try(InputStream in=connection.getInputStream();ByteArrayOutputStream out=new ByteArrayOutputStream()){byte[] buf=new byte[2048];int n;while((n=in.read(buf))!=-1)out.write(buf,0,n);return new JSONObject(out.toString("UTF-8"));}}finally{connection.disconnect();}
    }
    private void sendSample(){
        if(!enabled||!active)return;
        try{
            if(session.isEmpty()){JSONObject registration=post("/api/devices/register",new JSONObject().put("kind","phone"));session=registration.getString("session");seq=0;}
            double[] p=estimator.pose();JSONObject sensors;
            synchronized(sampleLock){sensors=new JSONObject().put("accelerometer",array(accel)).put("linear_acceleration",array(linear)).put("gyroscope",array(gyro)).put("magnetic",array(magnetic)).put("rotation_vector_available",hasRotation).put("linear_acceleration_available",hasLinear);}
            JSONObject gps=new JSONObject();Location fix=lastLocation;if(fix!=null)gps.put("latitude",fix.getLatitude()).put("longitude",fix.getLongitude()).put("accuracy",fix.getAccuracy()).put("timestamp_ms",fix.getTime());
            post("/api/devices/phone",new JSONObject().put("session",session).put("seq",++seq).put("pose",new JSONObject().put("east",p[0]).put("north",p[1]).put("yaw",p[2])).put("sensors",sensors).put("gps",gps));
            runOnUiThread(()->{if(!isFinishing()){status.setText("P01 已连接 · 自动同步中");readings.setText(String.format(Locale.US,"E %.2f m  ·  N %.2f m\n航向 %.1f°",p[0],p[1],Math.toDegrees(p[2])));}});
        }catch(Exception ex){runOnUiThread(()->{if(!isFinishing())status.setText("自动重连中："+ex.getMessage());});}
    }
}

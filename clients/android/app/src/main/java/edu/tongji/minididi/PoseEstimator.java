package edu.tongji.minididi;

/** Small ENU inertial estimator. Pure Java so its actual implementation can be
 * tested on a desktop JVM. It is a course prototype, not a SLAM implementation.
 * Device-to-world rotation is supplied by Android's rotation-vector fusion.
 */
public final class PoseEstimator {
    private double east, north, ve, vn, yaw;
    private double originLat=Double.NaN, originLon;
    private long previousTime=0;
    private double stillTime=0;
    public synchronized void reset() {
        east=north=ve=vn=yaw=stillTime=0; previousTime=0; originLat=Double.NaN;
    }
    public synchronized void update(float[] acceleration, float[] rotation,
                                    double azimuth, long timestampNanos) {
        yaw=azimuth;
        if(previousTime==0){previousTime=timestampNanos;return;}
        double dt=(timestampNanos-previousTime)*1e-9;previousTime=timestampNanos;
        if(dt<=0 || dt>0.2)return;
        double ae=rotation[0]*acceleration[0]+rotation[1]*acceleration[1]+rotation[2]*acceleration[2];
        double an=rotation[3]*acceleration[0]+rotation[4]*acceleration[1]+rotation[5]*acceleration[2];
        double norm=Math.sqrt(acceleration[0]*acceleration[0]+acceleration[1]*acceleration[1]+acceleration[2]*acceleration[2]);
        if(norm<0.13)stillTime+=dt;else stillTime=0;
        if(stillTime>0.45){ve=vn=0;return;} // zero-velocity update at rest
        if(Math.abs(ae)<0.06)ae=0;if(Math.abs(an)<0.06)an=0;
        // Light damping bounds integration drift; does not replace calibration.
        ve=clamp((ve+ae*dt)*Math.exp(-0.25*dt),-5,5);
        vn=clamp((vn+an*dt)*Math.exp(-0.25*dt),-5,5);
        east+=ve*dt;north+=vn*dt;
    }
    public synchronized void correctGps(double latitude,double longitude,float accuracy) {
        if(!Double.isFinite(latitude)||!Double.isFinite(longitude)||!Float.isFinite(accuracy)||accuracy<0||accuracy>20)return;
        if(Double.isNaN(originLat)){originLat=latitude;originLon=longitude;return;}
        double gpsE=Math.toRadians(longitude-originLon)*6378137*Math.cos(Math.toRadians(originLat));
        double gpsN=Math.toRadians(latitude-originLat)*6378137;
        double gain=accuracy<5?0.2:0.05;
        east=(1-gain)*east+gain*gpsE;north=(1-gain)*north+gain*gpsN;
    }
    public synchronized double[] pose(){return new double[]{east,north,yaw};}
    private static double clamp(double v,double lo,double hi){return Math.max(lo,Math.min(hi,v));}
}

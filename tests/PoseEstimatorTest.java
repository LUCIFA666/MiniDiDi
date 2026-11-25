import edu.tongji.minididi.PoseEstimator;
public class PoseEstimatorTest {
    static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
    public static void main(String[] args){
        PoseEstimator p=new PoseEstimator();float[] identity={1,0,0,0,1,0,0,0,1};long t=1000000000L;
        for(int i=0;i<100;i++){p.update(new float[]{0,0,0},identity,0,t);t+=20000000L;}
        check(Math.abs(p.pose()[0])<1e-9&&Math.abs(p.pose()[1])<1e-9,"stationary drift");
        for(int i=0;i<50;i++){p.update(new float[]{1,0,0},identity,0,t);t+=20000000L;}
        check(p.pose()[0]>.3&&Math.abs(p.pose()[1])<1e-9,"ENU acceleration integration");
        double before=p.pose()[0];for(int i=0;i<150;i++){p.update(new float[]{0,0,0},identity,0,t);t+=20000000L;}
        double atRest=p.pose()[0];for(int i=0;i<50;i++){p.update(new float[]{0,0,0},identity,0,t);t+=20000000L;}
        check(atRest>before&&Math.abs(p.pose()[0]-atRest)<1e-9,"zero-velocity update");
        p.correctGps(31,121,2);p.correctGps(31,121.00001,2);check(Double.isFinite(p.pose()[0]),"GNSS correction");
        p.reset();check(p.pose()[0]==0&&p.pose()[1]==0,"reset");
        System.out.println("PASS PoseEstimator: rest, ENU integration, zero velocity, GNSS, reset");
    }
}

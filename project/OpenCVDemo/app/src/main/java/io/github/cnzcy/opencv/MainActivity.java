package io.github.cnzcy.opencv;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import com.googlecode.tesseract.android.TessBaseAPI;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    private Bitmap bitmap;
    private ImageView imageView;
    private ImageView imageView2;
    private TessBaseAPI tess;
    private String path = Environment.getExternalStorageDirectory().getAbsolutePath() + "/tess";
    private TextView textView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        imageView = findViewById(R.id.imageView);
        imageView2 = findViewById(R.id.imageView2);
        textView = findViewById(R.id.textView);
        Button btn = findViewById(R.id.button);
        btn.setOnClickListener(this);
        bitmap = BitmapFactory.decodeResource(getResources(), R.drawable.sfz);

        // 初始化OCR
        tess = new TessBaseAPI();
        new Thread(){
            @Override
            public void run() {
                initTess();
            }
        }.start();
    }

    private void initTess() {
        InputStream is = null;
        FileOutputStream fos = null;
        try {
            is = getAssets().open("chi_sim.traineddata");
            File file = new File(path+File.separator+"tessdata"+File.separator+"chi_sim.traineddata");
            if(!file.exists()){
                file.getParentFile().mkdirs();
                fos = new FileOutputStream(file);
                byte[] buffer = new byte[2048];
                int len;
                while((len = is.read(buffer))!=-1){
                    fos.write(buffer, 0, len);
                }
                fos.close();
            }
            is.close();
            tess.init(path, "chi_sim");
            System.out.println("加载完成...");
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if(is != null){
                    is.close();
                }
                if(fos != null){
                    fos.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public native void bitmapDistinguishFromJNI(Bitmap bitmap);
    public native Bitmap findIdNumber(Bitmap bitmap, Bitmap.Config config);//Bitmap.Config.ARGB_8888

    @Override
    public void onClick(View v) {
        // bitmapDistinguishFromJNI(bitmap);
        Bitmap dstBitmap = findIdNumber(bitmap, Bitmap.Config.ARGB_8888);
        imageView2.setImageBitmap(dstBitmap);

        tess.setImage(dstBitmap);
        String text = tess.getUTF8Text();
        text = text.replaceAll("o", "0");
        textView.setText(text);

    }
}

package pp.compiler;
//***********
// Inspect
// disassembler ARM
////////////
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.widget.TextView;

public class Inspect extends Activity
{
	static final String FILE_NAME = "file_name";
	private String file_name;
	private TextView textview;
	private native String DisasmArm(int a, int c); // get a message from the compiler

	static
    {
        System.loadLibrary("disasm");
    }

	
	@Override
    protected void onCreate(Bundle savedInstanceState) 
    {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.inspect);
        Intent intent = getIntent();
        Bundle extras = intent.getExtras();
        
        if (extras==null) finish();   // ce cas est pour le moins embarassant
        
        file_name = extras.getString(FILE_NAME);
        setTitle(file_name);
        textview=(TextView)findViewById(R.id.inspect);
        textview.setMovementMethod(new ScrollingMovementMethod());
        load();
        //textview.setText("bonjour !");


    }
	

	private void load()
	{
		File file=new File(file_name);
    	try
    	{
    		int c=0, i=0;
    		FileInputStream in = new FileInputStream(file);
    		String txt= new String("");
    		c=in.read();
    		while (c!=-1)
    		{
    			int x = c+(in.read()<<8)+(in.read()<<16)+(in.read()<<24);

    			txt=txt+DisasmArm(i,x);
//    			Integer.toHexString(i)+" "+Integer.toHexString(x)+'\n';
    			i+=4;
    			c=in.read();
    		}
    		in.close();
    		textview.setText(txt,TextView.BufferType.SPANNABLE);

    	}
    	catch (IOException e){}	 // TODO traiter les exceptions
	}


}

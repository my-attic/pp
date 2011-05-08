package pp.compiler;
//-------------------------\
//                         !
//    Compile activity     !
//                         !
//-------------------------/

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;

public class Compile extends ConAct
{
	static final int ALERT_END_COMPILE = 1;
	static final int ALERT_FATAL = 2;

	public static final String FILE_NAME = "file_name";  // key of extras

	private native int compiler(String file_name);
	private native String message(); // get a message from the compiler
	private native int errPos();     // get the position of the error
	private native String currFileName();  // Name of the currently compiled file
	private native String currExeName();   // name of the executable created

    static
    {
        System.loadLibrary("build");
    }

	@Override
    protected void onCreate(Bundle savedInstanceState) 
    {
		String file_name;

        super.onCreate(savedInstanceState);
     
        Bundle extras = getIntent().getExtras();  

        if (extras!=null)
        {
        	file_name = extras.getString(FILE_NAME); 
            int e=compiler(file_name); // compilation
            if (e==0)  
            { // if no error, then all is right
                super.showDialog(ALERT_END_COMPILE);
            }
            else if (e>0)
            {  // else, launch editor and inform of the position and an error message 
            	edit(errPos(), true);
             }
            else // e<0 this is a fatal error from the compiler
            	// -1 : unable to load the source file
            	// 
            	// - unable to load the file in memory
            {
                showDialog(ALERT_FATAL);
            } 
 
        }
    }
	
	// Execute compiled file
    private void execute(String file_name)
    {
		finish();
    	Intent intent = new Intent(this,Exec.class);
    	intent.putExtra(Compile.FILE_NAME, file_name);
    	startActivity(intent); //, ACTIVITY_EXEC); //
    }
    
    // launch editor and finish current activity
    // pos : initial position of the cursor
    // err : display error alert with message ?
    private void edit(int pos, boolean err)
    {
    	String name=currFileName();
        finish();         
        Intent intent = new Intent(this, Piaf.class);
        intent.putExtra(Piaf.FILE_NAME, name);
        intent.putExtra(Piaf.IS_NEW, false);
        if (pos<0)
        {
        	if (Piaf.Name.equals(name))
        	{
        		pos=Piaf.cursorPosition;
        	}
        	else pos=0;
        }
        intent.putExtra(Piaf.INITIAL_POS, pos);
        if (err) intent.putExtra(Piaf.ERROR_MSG,message());
        startActivity(intent); 
    }
    
	@Override
    protected Dialog onCreateDialog(int id) 
	{
		AlertDialog.Builder builder;
        switch(id) 
        {
        case ALERT_END_COMPILE:
            builder = new AlertDialog.Builder(this);
           
            builder.setMessage(message())
                .setCancelable(false)
                .setTitle(R.string.info)
                .setIcon(R.drawable.info)
                .setPositiveButton("Done", new DialogInterface.OnClickListener() 
                {
                    public void onClick(DialogInterface dialog, int id)
                    {
                    }
                })

                .setNeutralButton("Edit", new DialogInterface.OnClickListener() 
                {
                    public void onClick(DialogInterface dialog, int id)
                    {
                        edit(-1, false);   
                    }
                })
                .setNegativeButton("Run", new DialogInterface.OnClickListener()
	             {
                	public void onClick(DialogInterface dialog, int id)
	                {
	                    execute(currExeName());
	                }
                });

            return builder.create();
        
        case ALERT_FATAL:
           builder = new AlertDialog.Builder(this);
          
           builder.setMessage(message())
               .setCancelable(false)
               .setTitle(R.string.error)
               .setIcon(R.drawable.danger)
               .setPositiveButton("Done", new DialogInterface.OnClickListener() 
               {
                   public void onClick(DialogInterface dialog, int id)
                   {
                       finish();
                   }
               })
               ;
           return builder.create();
        default:
        	return super.onCreateDialog(id);
        }
	}  
}




package pp.compiler;

//-----------------------//
// Piaf Is Almost Fozzy  //
//---------------------- //

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.TextView;


public class Piaf extends Activity
{

	static final String FILE_NAME = "file_name";     // extras indicators
	static final String IS_NEW = "is_new";
	static final String INITIAL_POS = "initial_pos";
	static final String ERROR_MSG = "error_msg";
	public static int cursorPosition=0;
	public static String Name="";  // used by the compile class
	
	// alert and dialog
	private final static int ALERT_ERROR = 0;
	final private static int DIALOG_NEW_FILE = 1;
	final private static int DIALOG_CHOOSE_FONT=2;

	private EditText edittext;
	private String file_name;
	private boolean is_new;
	private File file;
	private String error_msg;	
	
	// font size
	public final static int[] fontSize = {12,16,20,24};
	public static int fontSizeNb=1;
	
	// initialize the activity
	// assume that "file_name", "is_new" is assigned
	void Initialize()
	{
		Name=new String(file_name);  // Memorize the name
        setTitle(file_name);
        file = new File(file_name);
        //fontSizeNb=1;
        Update();
        if (is_new)
        {  // if it is a new file, then create it
        	// TODO proposer un dialogue si le fichier existe déjà
        	try
        	{
             	file.createNewFile();
             	edittext.setText(null);
        	}
        	catch (IOException e){} // TODO traiter les exceptions
        }
	}
	
	// update editor parameters
	private void Update()
	{
		edittext.setTextSize(fontSize[fontSizeNb]);
	}
	
	@Override
    protected void onCreate(Bundle savedInstanceState) 
    {
		int initial_position;

        super.onCreate(savedInstanceState);
        setContentView(R.layout.piaf);
        Intent intent = getIntent();
        Bundle extras = intent.getExtras();
        
        if (extras==null) finish();   // this should not occur as calling this intent assigns extras
        
        file_name = extras.getString(FILE_NAME);
        is_new = extras.getBoolean(IS_NEW);
        initial_position=extras.getInt(INITIAL_POS);
        error_msg=extras.getString(ERROR_MSG);
        edittext=(EditText)findViewById(R.id.piaf);
        Initialize();
        if (!is_new)
        {
        	load();
    		// visibly, set position 0 does not work
        	// when it is called now
     		edittext.setSelection(initial_position);
    		if (intent.hasExtra(ERROR_MSG))
    		{
    			error_msg=extras.getString(ERROR_MSG);
    			showDialog(ALERT_ERROR);
    		}
        }
    }
	
	private void load()
	{
    	try
    	{
    		int c,i=0;
    		FileInputStream in = new FileInputStream(file);
    		String txt= new String();
    		c=in.read();
    		while (c!=-1)
    		{
    			txt=txt+(char)c;
    			c=in.read();
    			i++;
    		}
    		in.close();
    		edittext.setText(txt,TextView.BufferType.SPANNABLE);
    		edittext.setSelection(i); // déplacer le curseur en dernière position
    		// It seems that set selection at position 0 does not works properly
    		// and for empty files, it is not possible to set at another position

    	}
    	catch (IOException e){}	 // TODO traiter les exceptions
	}

	private void save()
	{
		cursorPosition = edittext.getSelectionStart();
		try
		{
  		    FileOutputStream out=new FileOutputStream(file);
  		    String txt=edittext.getText().toString();
  		    if (txt.length()>0) out.write(txt.getBytes());
  		    out.close();
		}
		catch (Exception e){} // TODO traiter les exceptions
	}
	 
	@Override
	public void onPause()
	{
		super.onPause();
		save();
	}


    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        getMenuInflater().inflate(R.menu.piaf, menu);
        return true;
    }

    private void compile()
    {
        Intent intent = new Intent(this, Compile.class);
        intent.putExtra(Compile.FILE_NAME, file_name);
        finish();
        startActivity(intent); //, ACTIVITY_COMPILE);
    }

    
    @Override
    public boolean onOptionsItemSelected(MenuItem item) 
    {
        int id = item.getItemId();
        switch (id)
        {
        case R.id.menu_compile:
        	compile();
        	break;
        case R.id.menu_save:
        	save();
        	break;
        case R.id.menu_new:
        	showDialog(DIALOG_NEW_FILE);
        	break;
        case R.id.menu_fontsize:
        	showDialog(DIALOG_CHOOSE_FONT);
        	break;
        }
        return super.onOptionsItemSelected(item);
    } 

	
	@Override
    protected Dialog onCreateDialog(int id) 
	{
        switch(id) 
        {
        case ALERT_ERROR:
           AlertDialog.Builder builder = new AlertDialog.Builder(this);
          
           builder.setMessage(error_msg)
               .setCancelable(false)
               .setTitle(R.string.error)
               .setIcon(R.drawable.info)
               .setPositiveButton("Done", new DialogInterface.OnClickListener() 
               {
                   public void onClick(DialogInterface dialog, int id)
                   {
                       dialog.cancel();
                   }
               });
           AlertDialog alert=builder.create();
           return alert;
        case DIALOG_NEW_FILE:
        	final EditText editinput;
        	builder = new AlertDialog.Builder(this);
        	builder.setTitle("New source file name")  // TODO localiser la chaîne
        	.setIcon(R.drawable.newdoc)
        	.setView(editinput=new EditText(getApplicationContext()))
        	.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() 
        	{
               public void onClick(DialogInterface dialog, int id) 
               {
            	   save();
            	   file_name=editinput.getText().toString();
            	   is_new=true;
            	   Initialize();
                   dialog.cancel();
               }
            })
        	.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() 
        	{
               public void onClick(DialogInterface dialog, int id) 
               {
            	   dialog.cancel();
               }
            });
        	editinput.setText(file_name);
        	alert=builder.create();
        	return alert;
        case DIALOG_CHOOSE_FONT:
        	final CharSequence[] items = {"12", "16", "20", "24"};

        	builder = new AlertDialog.Builder(this);
        	builder.setTitle("Choose a font size");
        	
        	builder.setSingleChoiceItems(items, fontSizeNb, new DialogInterface.OnClickListener() {
        	    public void onClick(DialogInterface dialog, int item) 
        	    {
        	    	fontSizeNb=item;
        	    	Update();
        	    }
        	});
        	/* 
        	 * Alternative, make the font greater or smaller by clicking on a button
        	//builder.setMessage("Are you sure you want to exit?")
            builder//.setCancelable(false)
            .setPositiveButton("+", new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                     if (fontSizeNb<3) fontSizeNb++;
                     Update();
                }
            })
            .setNegativeButton("-", new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int id) {
                    if (fontSizeNb>0) fontSizeNb--;
                    Update();
                }
            });
            */
        	alert = builder.create();
        	return alert;

        default:
        	return null;
        }
	}

}

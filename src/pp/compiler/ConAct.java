package pp.compiler;

//-------------------------------------------------------------------------\
// The AonAct class is common for all the activities based on the console
// - Exec for running executables
// - Compile for compilation
// - a Shell (TODO)
//-------------------------------------------------------------------------/


import android.app.Activity;
//import android.app.AlertDialog;
//import android.app.Dialog;
//import android.content.DialogInterface;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Window;
import android.view.WindowManager;
//import android.view.Menu;
//import android.view.MenuItem;

class ConAct extends Activity
{
	//final private static int DIALOG_CHOOSE_FONT=0;


	Console con;
	int error;
	
	/////////////////////////////////////////////////
	// Fonctions qui sont utilisées par le loader
	/////////////////////////////////////////////////
	
	// affiche un caractère sur la console
    void emitChar(char c)
    {
    	con.emitChar(c); 
    }

    // attend qu'un caractère soit introduit sur la console
    char getChar()
    {
    	return con.getChar();
    }
	
    // efface l'écran et réinitialise le curseur en haut à gauche de l'écran
    public void clrscr()
    {
    	con.ClearScreen();
    }
    
    // get x position of cursor with origin 1
    public int WhereX()
    {
    	return con.xCursor + 1;
    }
    
    // get y position of cursor with origin 1
    //----------------------
    int WhereY()
    {
    	return con.yCursor + 1;
    }

    //  set cursor position to coordinates (x, y)
    // negative position is set to 1
    // too much large position set to maximum
    //-------------------------------------------
    public void GotoXY(int x, int y)
    {
    	if (x<=0) x=1;
    	else if (x>con.NbRows) x=con.NbRows;
    	if (y<=0) y=1;
    	else if (y>con.NB_LINES)y=con.NB_LINES;
    	con.setCursor(x-1, y-1);
    	con.makeCursorVisible();
    }
    
    // returns screen width
    //---------------------
    int ScreenWidth()
    {
    	return con.NbRows;
    }
    
    // returns screen height
    //---------------------
    int ScreenHeight()
    {
    	return con.NbLines;
    }
    
    
    ////////////////////////////////////////////////////////////////////
    
	@Override 
    protected void onCreate(Bundle savedInstanceState) 
    {

        super.onCreate(savedInstanceState);
        setContentView(R.layout.compile);
        con=(Console)findViewById(R.id.console);   // retrieve console view

        // get metrics in order to display correctly the caracters
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);
       
        con.InitConsole(this,Piaf.fontSize[Piaf.fontSizeNb] * metrics.density,0xffffff00,0xff0000cc);

        con.setFocusable(true);
        con.setFocusableInTouchMode(true);
        con.requestFocus();
 
        doScreenMode();
        
        con.updateSize();  // needed to initialize screen memory
       
    }
/*
	@Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        getMenuInflater().inflate(R.menu.console, menu);
        return true;
    }
 */
/*
 * 
    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
        int id = item.getItemId();
        if (id == R.id.menu_fontsize) 
        {
        	showDialog(DIALOG_CHOOSE_FONT);
        }
        return super.onOptionsItemSelected(item);
    } 
*/
	@Override
    public void onPause()
    {
        super.onPause();
        con.onPause();
    }
	
	@Override
    public void onResume() 
    { 
        super.onResume();

        con.onResume();


    } 
	private void doScreenMode()
    {
        Window win = getWindow();
       	win.setFlags(con.fullscreen?WindowManager.LayoutParams.FLAG_FULLSCREEN:0, 
        		     WindowManager.LayoutParams.FLAG_FULLSCREEN);
    } 
 /*
	void Update()
	{
        con.InitConsole(this,fontSize[fontSizeNb],0xffffff00,0xff0000cc);
	}
*/
	/*
	@Override
    protected Dialog onCreateDialog(int id) 
	{
		AlertDialog.Builder builder;
		AlertDialog alert;
        switch(id) 
        {
        case DIALOG_CHOOSE_FONT:
        	final CharSequence[] items = {"12", "16", "20", "24"};

        	builder = new AlertDialog.Builder(this);
        	builder.setTitle("Choose a font size"); // TODO Localize
        	
        	builder.setSingleChoiceItems(items, fontSizeNb, new DialogInterface.OnClickListener() {
        	    public void onClick(DialogInterface dialog, int item) 
        	    {
        	    	fontSizeNb=item;
        	    	Update();
        	    	con.updateSize();
        	    }
        	});
        	alert = builder.create();
        	return alert;

        default:
        	return null;

        
        }


    }
*/

}


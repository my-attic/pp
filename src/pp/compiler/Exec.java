package pp.compiler;
//--------------------------\
//                          |
//         Execute          |
//                          |
//--------------------------/



import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;

// the exec class extends the console activity
public class Exec extends ConAct
{
	final static int EXEC_RESULT = 1;
	private int err;
	String file_name;
	static
    {
        System.loadLibrary("loader"); 
    }
  
	private native int execute(String name);
//	private native int errno();  // récuprération du numéro d'erreur
	 
	
	
    protected void onCreate(Bundle savedInstanceState) 
    {
    	super.onCreate(savedInstanceState);
    	Bundle extras = getIntent().getExtras();  
        if (extras!=null) 
        {
        	file_name = extras.getString(Compile.FILE_NAME); // file to execute
        	
        }
    }
    
    // problème, l'affichage se fait uniquement à la fin de l'exécution
    
    // 1er essai pour résoudre le problème
    // exécuter dans "on resume" : ne change rien
    // 2ème essai : exécuter en postant un runnable
    //   - la console s'affiche au lancement, mais l'affichage n'est effectif que
    //     à la fin de l'exécution
    // 3ème essai : exécuter dans un thread -- ça plante. Il faut peut-être travailler
    // davantage le problème de l'accès simultané de deux threads à une mémoire commune
    // 4ème essai : utiliser postInvalidate() au lieu de Invalidate() -->
    // ça ne change rien !
    // Ca qui marche, c'est l'utilisation conjointe d'un thread et de postInvalidate
    
    
    //final Handler myHandler = new Handler();
    
    Runnable Execute= new Runnable()
    {   
    	public void run()
    	{
    		error=execute(file_name);
    	}
    };

    
    void ShowFinalDialog()
    {
    	if (error!=0)
    	{ 	// error <0 : load error
    		// error >0 : runtime error
    		// error==0 : ok
    		err=error;
//    		if (error<0) err=errno(); else err=error;
    		super.showDialog(EXEC_RESULT);
    	}
   	
    }
    final Handler myHandler=new Handler();
    Runnable FinalDialog= new Runnable()
    {   
    	public void run()
    	{
    		ShowFinalDialog();
    	}
    };

    private Thread ExecuteThread;
    
    @Override
    public void onResume() 
    { 
        super.onResume();
    	// myHandler.postDelayed(Execute,100);
        
		ExecuteThread = new Thread(new Runnable() {
		
				public void run()
		    	{
		    		error=execute(file_name);
		    		myHandler.postDelayed(FinalDialog, 100);
		    	}
		   
		    });

		ExecuteThread.start();

//        error=execute(file_name);


    }

    final static int DIVBYZERO = 1;
    final static int ERR_RESET = 8;
    final static int ERR_REWRITE = 4;
    final static int ERR_PUT = 10;

    @Override
    protected Dialog onCreateDialog(int id) 
	{
		AlertDialog.Builder builder;
		CharSequence msg;
        switch(id) 
        {
        case EXEC_RESULT:       	
            builder = new AlertDialog.Builder(this);
            switch (err)
            {
            case DIVBYZERO: // when error number is known, then display message
            	msg="Division by zero";
            	break;
            case ERR_RESET:
            	msg="Reset can't open file";
            	break;
            case ERR_REWRITE:
            	msg="Rewrite can't open file";
            	break;
           
            default: // else display error number
            	msg="exit value : "+err;
            }
            builder.setMessage(msg)
            .setCancelable(false)
            .setTitle(R.string.runtimeError)
            .setIcon(R.drawable.error)
            .setPositiveButton("Done", new DialogInterface.OnClickListener() 
            {
                public void onClick(DialogInterface dialog, int id)
                { 
                    dialog.cancel();
                }
            });


            return builder.create();

        default:
        	return super.onCreateDialog(id);
        }
	}


}


package pp.compiler;

//------------------------------\
//                              !
// module principal pour PéPé   !
//                              !
//------------------------------/


import java.io.File;
import java.util.ArrayList;
import java.util.List;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ListActivity;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.AdapterView.AdapterContextMenuInfo;

/*-----------------------------------------------------\
 * this object is a browser that launch the compiler   !
 * on .pas files    !                                  !
 * On .exe files, it loads and execute the executable  ! 
 * file                                                !
 * @author Archibald                                   !
 *                                                     !
\*----------------------------------------------------*/
public class PP extends ListActivity // implements View.OnCreateContextMenuListener
{
	//private final int ACTIVITY_COMPILE = 0;
	private final int ACTIVITY_PIAF = 1;
	//private final int ACTIVITY_EXEC = 2;
	private final int ACTIVITY_INSPECT = 3;
	private File currentDirectory;
	private List<String> directoryEntries = new ArrayList<String>();
	private final String INTERNALSTORAGEDIR="/data/data/pp.compiler";
	private final String EXTERNALDIR=Environment.getExternalStorageDirectory().getPath()+"/Pepe";

    int storageLocation=1;  // internal 0; external 1
    String new_file_name=null;

    // create the directory EXTERNALDIR/Pepe which is the default
    // directory where the source files are written
    // returns true if the directory exists or has properly been created
    // returns false if it is not possible to create this directory
    private boolean CreateDirectory()
    {
    	File dir=new File(EXTERNALDIR);
    	if (dir.exists())
    	{
    		if (dir.isDirectory()) return true;
    		if (!dir.delete()) return false;
    	}
    	return dir.mkdir();
    }
    
	@Override
    public void onCreate(Bundle savedInstanceState)
    {
      super.onCreate(savedInstanceState);
      registerForContextMenu(getListView());


      setTitle("Pépé le compiler");
      CreateDirectory(); // create the special PePe directory

      browseto(new File(EXTERNALDIR));
      // TODO mettre le point de départ dans les préférences
    }
	
	@Override
    public void onResume() 
    { 
        super.onResume();

        browseto(currentDirectory);

    } 
	
	private String alert_msg;
	// display an alert with this message
	private void alert(String msg)
	{
		alert_msg=msg;
		showDialog(ALERT_INFO);
	}
	
   
    // Check if a file name ends with ".pas"
    private boolean checkPas(String name)
    {
         return name.endsWith(".pas");
    }
    
    private boolean checkExe(String name)
    {
    	return name.endsWith(".exe");
    }
    
    // fill the list with directories names and Pascal files
    private void fill(File[] files) 
    {
    	this.directoryEntries.clear();  // clear the list
		
		if(this.currentDirectory.getParent() != null)
			if (!currentDirectory.getPath().equals(INTERNALSTORAGEDIR))
			this.directoryEntries.add("..");
	    for (File file : files)
	    {
	    	String name=file.getPath();
	    	//if ( file.isDirectory() )
	    	{
			   this.directoryEntries.add(name);
	    	}
		}
	    ArrayAdapter<String> directoryList = new ArrayAdapter<String>(this,
				R.layout.file_row, this.directoryEntries);
		
		this.setListAdapter(directoryList);
    }
    
    private void upOneLevel()
    {
		if(this.currentDirectory.getParent() != null)
		{
			this.browseto(this.currentDirectory.getParentFile());
		}
	}
    
    // browse a directory
    // The parameter "aDirectory" is supposed to be a directory
    private boolean browseto(final File aDirectory)
    {
    	//PPCompiler.setTitle(aDirectory.getAbsolutePath() + " :: " +  getString(R.string.app_name));
    	//if (aDirectory.isDirectory())
    	File[] files;
		this.currentDirectory = aDirectory;
		files=aDirectory.listFiles();
		if (files!=null)
		{
			fill(aDirectory.listFiles());
			return true;
		}
        alert("Access denied");
		return false;
    }
    
    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }
    
    @Override
	protected void onListItemClick(ListView l, View v, int position, long id)
    {
		String selectedFileString = this.directoryEntries.get((int)id);
		if(selectedFileString.equals(".."))
		{
			this.upOneLevel();
		}
		else 
		{
			File clickedFile = null;
		
			clickedFile = new File(this.directoryEntries.get((int)id));
												
					
			if(clickedFile != null)
			{
				if (clickedFile.isDirectory())
				{
					this.browseto(clickedFile);
				}
				else
				{

					if ( checkPas(selectedFileString) )
				    {
						compile(selectedFileString);
				    }
					else if (checkExe(selectedFileString))
					{
						execute(selectedFileString);
					}
				}
			}
		}
	}
    
    
	// compile
    //---------
    private void compile(String file_name)
    {
        Intent intent = new Intent(this, Compile.class);
        intent.putExtra(Compile.FILE_NAME, file_name);
        startActivity(intent); //, ACTIVITY_COMPILE);
    }
    
	// run
    //-----
    private void execute(String file_name)
    {
    	Intent intent = new Intent(this,Exec.class);
    	intent.putExtra(Compile.FILE_NAME, file_name);
    	startActivity(intent); //, ACTIVITY_EXEC); //
    }
    
    // Delete a file
    private String file_to_delete;


    // Edit a file given by its name.
    // The boolean 'new_file' tells if the file is new
    private void edit(String file_name, Boolean new_file)
    {
        Intent intent = new Intent(this, Piaf.class);
        intent.putExtra(Piaf.FILE_NAME, file_name);
        intent.putExtra(Piaf.IS_NEW, new_file);
        intent.putExtra(Piaf.INITIAL_POS, 0);
        startActivityForResult(intent, ACTIVITY_PIAF);
    }
    
    
    // inspect an executable file
    // display the ARM disassembling
    private void inspect(String file_name)
    {
    	Intent intent=new Intent(this, Inspect.class);
    	intent.putExtra(Inspect.FILE_NAME, file_name);
    	startActivityForResult(intent, ACTIVITY_INSPECT);
    }

    final private int EDIT_ID = 1;
    final private static int DELETE_ID = 2;
    final private static int BUILD_ID = 3;
    final private static int EXEC_ID = 4;
    final private static int INSPECT_ID = 5;

    // construction of the contextual menu
    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo)
    {
    	int selectionID = (int)((AdapterContextMenuInfo)menuInfo).id;//(int)getSelectedItemId();
		String selectedItemString = directoryEntries.get(selectionID);
    	super.onCreateContextMenu(menu,v,menuInfo);
    	menu.setHeaderTitle("file");
    	if (checkPas(selectedItemString))
    	{  // pour les fichiers .pas, :édition, compilation
    	   	menu.add(0,EDIT_ID, 0,R.string.ed);
    	   	menu.add(0,BUILD_ID,0,R.string.build);
    	   	
    	}
    	else if (checkExe(selectedItemString))
    	{  // pour les fichiers exe : run
    		menu.add(0,EXEC_ID,0,R.string.exec);
    		menu.add(0,INSPECT_ID,0,R.string.inspect);
    	}
    	if ((!selectedItemString.equals(".."))/*&& (!(new File(selectedItemString)).isDirectory())*/)
        {   // delete all, but the directory ".."
        	menu.add(0, DELETE_ID, 0, R.string.del);
        }
    }
    
    @Override
    public boolean onContextItemSelected(MenuItem item) 
    {
    	  AdapterContextMenuInfo info = (AdapterContextMenuInfo) item.getMenuInfo();
    	  switch (item.getItemId()) 
    	  {
    	  case EDIT_ID:
    	      edit(this.directoryEntries.get((int)info.id),false);
    	      return true;
    	  case DELETE_ID:
    	      file_to_delete=this.directoryEntries.get((int)info.id);
    	      showDialog(ALERT_DELETE);
    	      return true;
    	  case BUILD_ID:
    		  compile(this.directoryEntries.get((int)info.id));
    	      return true;
    	  case EXEC_ID:
    		  execute(this.directoryEntries.get((int)info.id));
    		  return true;
    	  case INSPECT_ID:
    		  inspect(this.directoryEntries.get((int)info.id));
    		  return true;
    	  default:
    	    return super.onContextItemSelected(item);
    	  }
    }

    //----------------
    // menu
    //----------------
    
    
    @Override
    public boolean onOptionsItemSelected(MenuItem item) 
    {
        int id = item.getItemId();
        switch (id)
        {
        case R.id.menu_storage:
        	showDialog(DIALOG_STORAGE_CHOICE);
        	break;
        case R.id.menu_new:
        	showDialog(DIALOG_NEW_FILE);
            break;
        }
        return super.onOptionsItemSelected(item);
    } 
    
    private int storageChoice;  // This variable is used
    // by the dialog for choosing the storage location
    // (internal or external) and for storing the choice
    // it is assigned to 'storageLocation' if the button 'OK' is pressed

    // dialogs and alerts
    //--------------------
    final private static int DIALOG_STORAGE_CHOICE = 0;
    final private static int DIALOG_NEW_FILE = 1;
    final private static int ALERT_INFO   = 2;
    final private static int ALERT_DELETE = 3;
    
    @Override
    protected Dialog onCreateDialog(int id) 
    {
    	AlertDialog.Builder builder;
    	AlertDialog alert;

 
        switch(id) 
        {
        case DIALOG_STORAGE_CHOICE:
            // TODO mettre ces dialogues en layout
        	final CharSequence[] items = {"Internal", "External"};

        	builder = new AlertDialog.Builder(this);

        	builder.setTitle("Choose a storage location")
        	.setIcon(R.drawable.disquettes)
        	.setSingleChoiceItems(items, storageLocation, new DialogInterface.OnClickListener() 
        	{
        	    public void onClick(DialogInterface dialog, int item) 
        	    {
        	        storageChoice=item;
        	    }
        	})
        	.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
               public void onClick(DialogInterface dialog, int id) 
               {
                   dialog.cancel();
                   if ( browseto (new File((storageChoice==0)?INTERNALSTORAGEDIR:EXTERNALDIR)) )
                	   storageLocation=storageChoice;  // ne change l'état que si on a réussi à browser
                      // cela de devrait normalement pas arriver
               }
            })
        	.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() 
        	{
               public void onClick(DialogInterface dialog, int id) 
               {
                    dialog.cancel();
               }
            });
        	alert = builder.create();
        	
            return alert;
        case DIALOG_NEW_FILE:
        	final EditText edittext;
        	builder = new AlertDialog.Builder(this);
        	builder.setTitle("New source file name")  // TODO localiser la chaîne
        	.setIcon(R.drawable.newdoc)
        	.setView(edittext=new EditText(getApplicationContext()))
        	.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() 
        	{
               public void onClick(DialogInterface dialog, int id) 
               {
            	   new_file_name=edittext.getText().toString();
                   dialog.cancel();
                   edit(currentDirectory+"/"+new_file_name,true);
               }
            })
        	.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() 
        	{
               public void onClick(DialogInterface dialog, int id) 
               {
            	   new_file_name=null;
                   dialog.cancel();
               }
            });
        	alert=builder.create();
        	return alert;
        case ALERT_INFO:
        	builder = new AlertDialog.Builder(this);
        	builder.setTitle("Information !")
        	.setIcon(R.drawable.info)
        	.setMessage(alert_msg)
        	.setNegativeButton("Done", new DialogInterface.OnClickListener() 
        	{
               public void onClick(DialogInterface dialog, int id) 
               {
                   dialog.cancel();
               }
            });
        	alert=builder.create();
        	return alert;
        case ALERT_DELETE:
        	builder = new AlertDialog.Builder(this);
        	builder.setTitle("Warning !")
        	.setIcon(R.drawable.danger)
        	.setMessage("Are you sure to delete \""+file_to_delete+"\" ?")
        	.setPositiveButton("Yes", new DialogInterface.OnClickListener() 
        	{
               public void onClick(DialogInterface dialog, int id) 
               {
               		File file=new File(file_to_delete);
               		if (!file.delete())
               		{
               			alert_msg="Fail to delete \""+file_to_delete+"\"";
               			showDialog(ALERT_INFO);
               		}
               		browseto(currentDirectory); // raffraichir le répertoire courant
                    dialog.cancel();
               }
            })
        	.setNegativeButton("No", new DialogInterface.OnClickListener() 
        	{
               public void onClick(DialogInterface dialog, int id) 
               {
                   dialog.cancel();
               }
            });
        	alert=builder.create();
        	return alert;
        default:
            return null;
        }

    }
    
}

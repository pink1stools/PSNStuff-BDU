// ---------------------------------------------------------------------
// CaptainCPS-X's gamePKG Tool
// ---------------------------------------------------------------------
struct c_pkglist
{
	char		path[256];	// full pkg path
	char		ID[10];	// full pkg path
	char		CID[256];	// full pkg path
	char		Typ[10];	// full pkg path
	char		Region[10];	// full pkg path
	char		title[256];	// pkg filename
	char		rapname[256];	// pkg filename
	char     	rapdata[256];	// pkg filename
	char     	rapdatat[256];	// pkg filename
	char     	rapdatab[256];	// pkg filename
	
	uint64_t    rapdata2;	// pkg filename
	uint64_t    rapdata3;	// pkg filename
	char		postedby[256];	// pkg filename
	bool		bInternal;	// HDD / USB PKG
	bool		bQueued;	// Queued status
	uint64_t	nSize;		// size in bytes
	int			nPKGID;		// Ex. 80000000
};

#define STATUS_NORMAL			0
#define STATUS_COPY_START		1
#define STATUS_COPY_OK			2
#define STATUS_COPY_ERROR		10
#define STATUS_RM_QUEUE_START	4
#define STATUS_RM_QUEUE_OK		5
#define STATUS_START			3

class c_gamePKG 
{
public:

	c_gamePKG();

	int		nSelectedPKG;
	int		nPKGListTop;
	int		nTotalPKG;
	bool	bDeleting;	
	int		nPKGID;

	int		nStatus;
	
	c_pkglist	*pkglst;

	int			QueuePKG();
	void		RemoveFromQueue();
	void		RemovePKG(int nId);
	int			CreatePDBFiles();
	int			CreatePKGPDBFiles();
	int			CreateVideoPDBFiles();
	int			CreateIMGPDBFiles();
	int			DeletePDBFiles(int nId);
	int			RemoveAllDirFiles(char* szDirectory);
	int			RemovePKGDir(int nId);
	int			GetPKGDirId();
	void		RefreshPKGList();
	uint64_t	GetPKGSize(char* szFilePath);
	int			ParsePKGList();
	int			ParsePKGListFile();
	
	void		Frame();
	void		DisplayFrame();
	void		InputFrame();
	void		DlgDisplayFrame();
	//void        shutdown_system(u8 mode);

private:
	// ...
};

extern c_gamePKG* gamePKG;

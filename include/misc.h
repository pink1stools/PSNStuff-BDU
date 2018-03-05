// ---------------------------------------------------------------------
// CaptainCPS-X's gamePKG Tool
// ---------------------------------------------------------------------

int	_FcCompareStruct(const void *pStructA, const void *pStructB);
int _FcCompareStr(const void *pStrA, const void *pStrB);
char* GetNumWithCommas(int64_t nNumber);
char* toLowerCase(char* pszStr, size_t nLen);
char* toUpperCase(char* pszStr, size_t nLen);
double GetByteUnit(int64_t nSize);
const char* GetByteUnitStr(int64_t nSize);
uint64_t hex_to_u64(const char* hex_str);

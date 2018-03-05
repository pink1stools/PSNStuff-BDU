void psgroove_main(int enable);
void hermes_payload_341();
void hermes_payload_355(int enable);

int patch_sys_storage(void);
int unpatch_sys_storage(void);

void poke_lv1(u64 _addr, u64 _val);
u64 peek_lv1(u64 _addr);
const MAXNAMELEN = 255;

typedef string nametype<MAXNAMELEN>;

typedef struct namenode *namelist;

struct namenode {
    nametype name;
    namelist next;
};

union readdir_res switch (int errno) {
case 0:
    namelist list;
default:
    void; /* error ocurred */
};

program DIRPROG {
    version DIRVERS{
        readdir_res
            READDIR(nametype) = 1;
    } = 1;
} = 76;

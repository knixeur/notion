/*
 * ion/mod_statusbar/ion-statusd/ion-statusd.c
 *
 * Copyright (c) Tuomo Valkonen 2004-2006.
 *
 * Ion is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libtu/util.h>
#include <libtu/optparser.h>
#include <libtu/errorlog.h>
#include <libtu/locale.h>
#include <libtu/misc.h>
#include <libextl/readconfig.h>
#include <libmainloop/select.h>
#include <libmainloop/signal.h>
#include <libmainloop/defer.h>

#ifndef CF_NO_LOCALE
#include <locale.h>
#endif

#include "../../version.h"


static OptParserOpt ion_opts[]={
    /*{OPT_ID('d'), "display",  OPT_ARG, "host:dpy.scr",
     DUMMY_TR("X display to use")},*/

    {'c', "conffile", OPT_ARG, "config_file",
     DUMMY_TR("Configuration file")},

    {'s', "searchdir", OPT_ARG, "dir",
     DUMMY_TR("Add directory to search path")},

    /*{OPT_ID('s'), "session",  OPT_ARG, "session_name",
     DUMMY_TR("Name of session (affects savefiles)")},*/

    {'h', "help", 0, NULL,
     DUMMY_TR("Show this help")},

    {'V', "version", 0, NULL,
     DUMMY_TR("Show program version")},

    {OPT_ID('a'), "about", 0, NULL,
     DUMMY_TR("Show about text")},

    {'q', "quiet", 0, NULL,
     DUMMY_TR("Quiet mode")},

    {'m', "meter", OPT_ARG, "meter_module",
     DUMMY_TR("Load a meter module")},

    END_OPTPARSEROPTS
};


static const char statusd_copy[]=
    "Ion-statusd " ION_VERSION ", copyright (c) Tuomo Valkonen 2004-2005.";


static const char statusd_license[]=DUMMY_TR(
    "This program is free software; you can redistribute it and/or\n"
    "modify it under the terms of the GNU Lesser General Public\n"
    "License as published by the Free Software Foundation; either\n"
    "version 2.1 of the License, or (at your option) any later version.\n"
    "\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU\n"
    "Lesser General Public License for more details.\n");


/* new_informs=TRUE because we should always print period when
 * initialisation is done.
 */
static bool new_informs=TRUE;
static ExtlTab configtab;

static void help()
{
    int i;
    printf(TR("Usage: %s [options]\n\n"), libtu_progname());
    for(i=0; ion_opts[i].descr!=NULL; i++)
        ion_opts[i].descr=TR(ion_opts[i].descr);
    optparser_printhelp(OPTP_MIDLONG, ion_opts);
    printf("\n");
}


static void flush_informs()
{
    if(new_informs){
        printf(".\n");
        fflush(stdout);
        new_informs=FALSE;
    }
}


static void mainloop()
{
    sigset_t trapset;

    sigemptyset(&trapset);
    sigaddset(&trapset, SIGALRM);
    sigaddset(&trapset, SIGCHLD);
    mainloop_trap_signals(&trapset);

    while(1){
        int kill_sig=mainloop_check_signals();
        if(kill_sig!=0 && kill_sig!=SIGUSR1){
            if(kill_sig==SIGTERM)
                exit(EXIT_FAILURE);
            else
                kill(getpid(), kill_sig);
        }

        mainloop_execute_deferred();

        flush_informs();

        mainloop_select();
    }
}


extern bool statusd_register_exports();
extern void statusd_unregister_exports();


int main(int argc, char*argv[])
{
    const char *mod=NULL;
    char *mod2=NULL;
    int loaded=0;
    int opt;
    bool quiet=FALSE;

#ifndef CF_NO_LOCALE
    if(setlocale(LC_ALL, "")==NULL)
        warn("setlocale() call failed.");
#endif

    configtab=extl_table_none();

    libtu_init(argv[0]);
    extl_init();

    if(!statusd_register_exports())
        return EXIT_FAILURE;

    extl_add_searchdir(EXTRABINDIR);
    extl_add_searchdir(MODULEDIR);
    extl_add_searchdir(ETCDIR);
    extl_add_searchdir(SHAREDIR);
    extl_add_searchdir(LCDIR);
    extl_set_userdirs("notion");

    optparser_init(argc, argv, OPTP_MIDLONG, ion_opts);

    extl_read_config("ioncore_luaext", NULL, TRUE);

    while((opt=optparser_get_opt())){
        switch(opt){
        /*case OPT_ID('d'):
            display=optparser_get_arg();
            break;*/
        case 's':
            extl_add_searchdir(optparser_get_arg());
            break;
        /*case OPT_ID('s'):
            extl_set_sessiondir(optparser_get_arg());
            break;*/
        case 'h':
            help();
            return EXIT_SUCCESS;
        case 'V':
            printf("%s\n", ION_VERSION);
            return EXIT_SUCCESS;
        case OPT_ID('a'):
            printf("%s\n\n%s", statusd_copy, TR(statusd_license));
            return EXIT_SUCCESS;
        case 'c':
            {
                ExtlTab t;
                const char *f=optparser_get_arg();
                if(extl_read_savefile(f, &t)){
                    extl_unref_table(configtab);
                    configtab=t;
                }else{
                    warn(TR("Unable to load configuration file %s"), f);
                }
            }
            break;
        case 'q':
            quiet=TRUE;
            break;
        case 'm':
            mod=optparser_get_arg();
            if(strchr(mod, '/')==NULL && strchr(mod, '.')==NULL){
                mod2=scat("statusd_", mod);
                if(mod2==NULL)
                    return EXIT_FAILURE;
                mod=mod2;
            }
            if(extl_read_config(mod, NULL, !quiet))
                loaded++;
            if(mod2!=NULL)
                free(mod2);
            break;
        default:
            warn(TR("Invalid command line."));
            help();
            return EXIT_FAILURE;
        }
    }

    if(loaded==0 && !quiet){
        warn(TR("No meters loaded."));
        return EXIT_FAILURE;
    }

    mainloop();

    return EXIT_SUCCESS;
}


/*EXTL_DOC
 * Inform that meter \var{name} has value \var{value}.
 */
EXTL_EXPORT
void statusd_inform(const char *name, const char *value)
{
    printf("%s: %s\n", name, value);
    new_informs=TRUE;
}


/*EXTL_DOC
 * Get configuration table for module \var{name}
 */
EXTL_EXPORT
ExtlTab statusd_get_config(const char *name)
{
    if(name==NULL){
        return extl_ref_table(configtab);
    }else{
        ExtlTab t;
        if(extl_table_gets_t(configtab, name, &t))
            return t;
        else
            return extl_create_table();
    }
}


/*EXTL_DOC
 * Get last file modification time.
 */
EXTL_EXPORT
double statusd_last_modified(const char *fname)
{
    struct stat st;

    if(fname==NULL)
        return (double)(-1);

    if(stat(fname, &st)!=0){
        /*warn_err_obj(fname);*/
        return (double)(-1);
    }

    return (double)(st.st_mtime>st.st_ctime ? st.st_mtime : st.st_ctime);
}

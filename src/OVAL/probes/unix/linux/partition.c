/**
 * @file   partition.c
 * @brief  partition probe implementation
 * @author "Daniel Kopecek" <dkopecek@redhat.com>
 *
 */

/*
 * Copyright 2011 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "Daniel Kopecek" <dkopecek@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <sys/statvfs.h>
#include <probe-api.h>
#include <mntent.h>
#include <pcre.h>

#define MTAB_PATH "/etc/mtab"
#define MTAB_LINE_MAX 4096

static int collect_item(SEXP_t *probe_out, struct mntent *mnt_ent)
{
        SEXP_t *item;
        char   *uuid, *tok, *save = NULL, **mnt_opts;
        uint8_t mnt_ocnt;
        struct statvfs stvfs;

        /*
         * Get FS stats
         */
        if (statvfs(mnt_ent->mnt_dir, &stvfs) != 0)
                return (-1);

        /*
         * Get UUID
         */
        uuid = "";

        /*
         * Create a NULL-terminated array from the mount options
         */
        mnt_opts = oscap_alloc(sizeof(char *) * 2);
        mnt_ocnt = 0;

        tok = strtok_r(mnt_ent->mnt_opts, ",", &save);

        do {
                mnt_opts[++mnt_ocnt - 1] = tok;
                mnt_opts = oscap_realloc(mnt_opts,
                                         sizeof(char *) * (mnt_ocnt + 1));
                mnt_opts[mnt_ocnt] = NULL;
        } while ((tok = strtok_r(NULL, ",", &save)) != NULL);

        /*
         * Create the item
         */
        item = probe_item_create(OVAL_LINUX_PARTITION, NULL,
                                 "mount_point",   OVAL_DATATYPE_STRING,   mnt_ent->mnt_dir,
                                 "device",        OVAL_DATATYPE_STRING,   mnt_ent->mnt_fsname,
                                 "uuid",          OVAL_DATATYPE_STRING,   uuid,
                                 "fs_type",       OVAL_DATATYPE_STRING,   mnt_ent->mnt_type,
                                 "mount_options", OVAL_DATATYPE_STRING_M, mnt_opts,
                                 "total_space",   OVAL_DATATYPE_INTEGER, (int)stvfs.f_blocks,
                                 "space_used",    OVAL_DATATYPE_INTEGER, (int)(stvfs.f_blocks - stvfs.f_bfree),
                                 "space_left",    OVAL_DATATYPE_INTEGER, (int)stvfs.f_bfree,
                                 NULL);

        probe_cobj_add_item(probe_out, item);
        SEXP_free(item);
        oscap_free(mnt_opts);

        return (0);
}

int probe_main(SEXP_t *probe_in, SEXP_t *probe_out, void *probe_arg, SEXP_t *filters)
{
        int probe_ret = 0;
        SEXP_t *mnt_entval, *mnt_opval;
        char    mnt_path[PATH_MAX];
        oval_operation_t mnt_op;
        FILE *mnt_fp;

        mnt_entval = probe_obj_getent(probe_in, "mount_point", 1);

        if (mnt_entval == NULL)
                return (PROBE_ENOENT);

        mnt_opval = probe_ent_getattrval(mnt_entval, "operation");

        if (mnt_opval != NULL) {
                mnt_op = (oval_operation_t)SEXP_number_geti(mnt_opval);
                SEXP_free(mnt_opval);
        } else
                mnt_op = OVAL_OPERATION_EQUALS;

        if (!SEXP_stringp(mnt_opval)) {
                SEXP_free(mnt_entval);
                return (PROBE_EINVAL);
        }

        SEXP_string_cstr_r(mnt_entval, mnt_path, sizeof mnt_path);
        SEXP_free(mnt_entval);

        mnt_fp = fopen(MTAB_PATH, "r");

        if (mnt_fp != NULL) {
                char buffer[MTAB_LINE_MAX];
                struct mntent mnt_ent, *mnt_entp;

                pcre *re = NULL;
                const char *estr = NULL;
                int eoff = -1;

                if (mnt_op == OVAL_OPERATION_PATTERN_MATCH) {
                        re = pcre_compile(mnt_path, PCRE_UTF8, &estr, &eoff, NULL);

                        if (re == NULL) {
                                endmntent(mnt_fp);
                                return (PROBE_EINVAL);
                        }
                }

                while ((mnt_entp = getmntent_r(mnt_fp, &mnt_ent,
                                               buffer, sizeof buffer)) != NULL)
                {
                        if (mnt_op == OVAL_OPERATION_EQUALS) {
                                if (strcmp(mnt_entp->mnt_dir, mnt_path) == 0) {
                                        collect_item(probe_out, mnt_entp);
                                        break;
                                }
                        } else if (mnt_op == OVAL_OPERATION_PATTERN_MATCH) {
                                int rc;

                                rc = pcre_exec(re, NULL, mnt_entp->mnt_dir,
                                               strlen(mnt_entp->mnt_dir), 0, 0, NULL, 0);

                                if (rc == 0)
                                        collect_item(probe_out, mnt_entp);

                                /* XXX: check for pcre_exec error */
                        }
                }

                endmntent(mnt_fp);

                if (mnt_op == OVAL_OPERATION_PATTERN_MATCH)
                        pcre_free(re);
        }

        return (probe_ret);
}

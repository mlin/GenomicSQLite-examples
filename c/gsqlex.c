#include "genomicsqlite.h"
#include <errno.h>
#include <malloc.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

int load_record(sqlite3_stmt *insert, char *line) {
    char *chromosome = strsep(&line, "\t");
    char *begin_pos = strsep(&line, "\t");
    char *end_pos = strsep(&line, "\t");
    if (!(chromosome && begin_pos && end_pos && line)) {
        fprintf(stderr, "Invalid GFF\n");
        return SQLITE_ERROR;
    }
    int rc = sqlite3_reset(insert);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to reset insert statment\n");
        return rc;
    }
    rc = sqlite3_bind_text(insert, 1, chromosome, -1, 0);
    rc = rc != SQLITE_OK ? rc : sqlite3_bind_text(insert, 2, begin_pos, -1, 0);
    rc = rc != SQLITE_OK ? rc : sqlite3_bind_text(insert, 3, end_pos, -1, 0);
    rc = rc != SQLITE_OK ? rc : sqlite3_bind_text(insert, 4, line, -1, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to bind parameters to insert statment\n");
        return rc;
    }
    rc = sqlite3_step(insert);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Unable to insert record, code = %d\n", rc);
        return rc;
    }
    return SQLITE_OK;
}

int load(const char *input_gff, const char *output_db) {
    FILE *infile = fopen(input_gff, "r");
    if (!infile) {
        fprintf(stderr, "Unable to open %s\n", input_gff);
        return 1;
    }

    sqlite3 *dbconn;
    char *zErrMsg = 0;
    int rc =
        genomicsqlite_open(output_db, &dbconn, &zErrMsg, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE,
                           "{\"unsafe_load\":true}");
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open %s: %s\n", output_db, zErrMsg ? zErrMsg : "");
        sqlite3_free(zErrMsg);
        fclose(infile);
        return rc;
    }

    rc = sqlite3_exec(dbconn, "BEGIN", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to BEGIN transaction: %s\n", zErrMsg ? zErrMsg : "");
        sqlite3_free(zErrMsg);
        fclose(infile);
        sqlite3_close_v2(dbconn);
        return rc;
    }

    rc = sqlite3_exec(dbconn,
                      "CREATE TABLE gff("
                      "chromosome TEXT COLLATE UINT,"
                      "begin_pos INTEGER,"
                      "end_pos INTEGER,"
                      "line TEXT)",
                      0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (zErrMsg) {
            fprintf(stderr, "%s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        fclose(infile);
        sqlite3_close_v2(dbconn);
        return rc;
    }

    sqlite3_stmt *insert = 0;
    rc = sqlite3_prepare_v2(dbconn,
                            "INSERT INTO gff(chromosome,begin_pos,end_pos,line) VALUES(?,?,?,?)",
                            -1, &insert, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to prepare INSERT statement\n");
        fclose(infile);
        sqlite3_close_v2(dbconn);
        return rc;
    }

    int count = 0;
    char *line = 0;
    size_t n = 0;
    ssize_t sz;
    errno = 0;
    while ((sz = getline(&line, &n, infile)) >= 0) {
        for (; sz && line[sz - 1] == '\n'; line[--sz] = 0)
            ;
        if (sz) {
            rc = load_record(insert, line);
            if (rc != SQLITE_OK) {
                free(line);
                fclose(infile);
                sqlite3_close_v2(dbconn);
                return rc;
            }
            count++;
        }
    }
    free(line);
    fclose(infile);
    if (errno) {
        fprintf(stderr, "Unable to read %s, errno = %d\n", input_gff, errno);
        sqlite3_close_v2(dbconn);
        return errno;
    }
    rc = sqlite3_finalize(insert);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to finalize insert statement\n");
        sqlite3_close_v2(dbconn);
        return rc;
    }

    char *gri_sql = create_genomic_range_index_sql("gff", "chromosome", "begin_pos", "end_pos", -1);
    if (!gri_sql || *gri_sql == 0) {
        /* see libgenomicsqlite.h for calling convention details*/
        fprintf(stderr, "Unable to create_genomic_range_index_sql: %s\n",
                gri_sql ? gri_sql + 1 : "");
        sqlite3_close_v2(dbconn);
        return SQLITE_ERROR;
    }
    rc = sqlite3_exec(dbconn, gri_sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to create GRI: %s\n", zErrMsg ? zErrMsg : "");
        sqlite3_free(zErrMsg);
        sqlite3_close_v2(dbconn);
        return rc;
    }

    rc = sqlite3_exec(dbconn, "COMMIT", 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to COMMIT transaction: %s\n", zErrMsg ? zErrMsg : "");
        sqlite3_free(zErrMsg);
        sqlite3_close_v2(dbconn);
        return rc;
    }
    rc = sqlite3_close_v2(dbconn);
    if (rc == SQLITE_OK) {
        printf("Loaded %d GFF records\n", count);
    } else {
        fprintf(stderr, "Unable to close database\n");
    }
    return rc;
}

int query(const char *dbfile, const char *range) {
    sqlite3 *dbconn;
    char *zErrMsg = 0;
    int rc = genomicsqlite_open(dbfile, &dbconn, &zErrMsg, SQLITE_OPEN_READONLY, "{}");
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to open %s: %s\n", dbfile, zErrMsg ? zErrMsg : "");
        sqlite3_free(zErrMsg);
        return rc;
    }

    sqlite3_stmt *query;
    rc = sqlite3_prepare_v2(dbconn,
                            "SELECT chromosome, begin_pos, end_pos, line FROM gff WHERE _rowid_ IN "
                            "  genomic_range_rowids('gff',parse_genomic_range_sequence(?1),"
                            "    parse_genomic_range_begin(?1),parse_genomic_range_end(?1))",
                            -1, &query, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to prepare query\n");
        sqlite3_close_v2(dbconn);
        return rc;
    }
    rc = sqlite3_bind_text(query, 1, range, -1, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Unable to bind parameter to insert statment\n");
        sqlite3_close_v2(dbconn);
        return rc;
    }
    while ((rc = sqlite3_step(query)) == SQLITE_ROW) {
        printf("%s\t%s\t%s\t%s\n", (const char *)sqlite3_column_text(query, 0),
               (const char *)sqlite3_column_text(query, 1),
               (const char *)sqlite3_column_text(query, 2),
               (const char *)sqlite3_column_text(query, 3));
    }
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Unable to query, code = %d\n", rc);
        sqlite3_close_v2(dbconn);
        return rc;
    }

    return sqlite3_close_v2(dbconn);
}

int main(int argc, char **argv) {
    char *zErrMsg = 0;
    int rc = GENOMICSQLITE_C_INIT(&zErrMsg);
    if (rc != SQLITE_OK) {
        if (zErrMsg) {
            fprintf(stderr, "%s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        return rc;
    }

    if (!strcmp(argv[1], "load")) {
        return load(argv[2], argv[3]);
    }
    if (!strcmp(argv[1], "query")) {
        return query(argv[2], argv[3]);
    }
    return 0;
}

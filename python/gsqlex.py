#!/usr/bin/env python3

import sys
import sqlite3
import genomicsqlite


def load(input_gff, output_db):
    # create compressed database file and schema therein
    dbconn: sqlite3.Connection = genomicsqlite.connect(output_db, unsafe_load=True)
    dbconn.executescript(
        """CREATE TABLE gff(
            chromosome TEXT COLLATE UINT,
            begin_pos INTEGER,
            end_pos INTEGER,
            line TEXT)"""
    )

    # fill in the GFF lines
    count = 0
    with open(input_gff) as infile:
        for line in infile:
            line = line.strip()
            fields = line.split("\t")
            chromosome = fields[0]
            begin_pos = int(fields[1]) - 1
            end_pos = int(fields[2])
            dbconn.execute(
                "INSERT INTO gff(chromosome,begin_pos,end_pos,line) VALUES(?,?,?,?)",
                (chromosome, begin_pos, end_pos, "\t".join(fields[3:])),
            )
            count += 1
    print(f"Loaded {count} GFF records")

    # create Genomic Range Index (GRI) on the GFF records
    create_gri_sql = genomicsqlite.create_genomic_range_index_sql(
        dbconn, "gff", "chromosome", "begin_pos", "end_pos"
    )
    dbconn.executescript(create_gri_sql)

    # COMMIT
    dbconn.commit()


def query(db, range_txt):
    # open compressed database for reading
    dbconn: sqlite3.Connection = genomicsqlite.connect(db, read_only=True)

    # query the genomic range using in-SQL helper functions for GRI and parsing the range string
    cursor = dbconn.execute(
        """SELECT chromosome, begin_pos, end_pos, line FROM gff WHERE _rowid_ IN
                genomic_range_rowids(
                    'gff',
                    parse_genomic_range_sequence(?1),
                    parse_genomic_range_begin(?1),
                    parse_genomic_range_end(?1))""",
        (range_txt,),
    )

    # print results
    for row in cursor:
        print("\t".join(str(elt) for elt in row))


def main(argv):
    if argv[1] == "load":
        load(argv[2], argv[3])
    elif argv[1] == "query":
        query(argv[2], argv[3])


if __name__ == "__main__":
    main(sys.argv)

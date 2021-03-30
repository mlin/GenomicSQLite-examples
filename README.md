# GenomicSQLite-examples

This repo exhibits a simple example program using [Genomics Extension for SQLite](https://github.com/mlin/GenomicSQLite), implemented in several supported programming languages. It's a minimal starting point to fork into your own program. See the [Programming Guide](https://mlin.github.io/GenomicSQLite) for full documentation.

Each implementation provides two subcommands:

1. `gsqlex load exons.gff exons.db` imports a small [GFF file](https://en.wikipedia.org/wiki/General_feature_format) into a new database, indexed by genomic range
2. `gsqlex query exons.db chr17:43,104,800-43,105,000` prints the records overlapping the given genomic range

General requirements: x86-64, Linux or macOS, installed development package for SQLite 3.31.0+.

### Python (3.6+)

```
pip3 install genomicsqlite

git clone https://github.com/mlin/GenomicSQLite-examples.git
cd GenomicSQLite-examples/python

python3 gsqlex.py load ../exons.gff exons.db
python3 gsqlex.py query exons.db chr17:43,104,800-43,105,000
```

### C

```
git clone https://github.com/mlin/GenomicSQLite-examples.git
cd GenomicSQLite-examples/c

wget https://github.com/mlin/GenomicSQLite/releases/download/v0.8.0/GenomicSQLite-v0.8.0.zip
unzip GenomicSQLite-v0.8.0.zip

cc -o gsqlex -L$(pwd) gsqlex.c -lsqlite3 -lgenomicsqlite -Wl,-rpath,\$ORIGIN

./gsqlex load ../exons.gff exons.db
./gsqlex query exons.db chr17:43,104,800-43,105,000
```

# GenomicSQLite-examples

This repo exhibits a small example program using [Genomics Extension for SQLite](https://github.com/mlin/GenomicSQLite), implemented in several supported programming languages. It's meant as a minimal starting point for you to fork into your own program. See the [Programming Guide](https://mlin.github.io/GenomicSQLite) for full documentation.

The program `gsqlex load exons.gff exons.db` imports a small [GFF file](https://en.wikipedia.org/wiki/General_feature_format) into a database, indexed by genomic range. Then, `gsqlex query exons.db chr17:43,104,800-43,105,000` prints the records overlapping the given genomic range.

All of the following example invocations begin from the root directory of this repository.

### Python

```
$ pip install genomicsqlite
$ python/gsqlex.py load exons.gff exons.db
$ python/gsqlex.py query exons.db chr17:43,104,800-43,105,000
```

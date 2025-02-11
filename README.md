## filterx

A simple and lightweight library for filtering tabular data with a simple and intuitive syntax.

## Installation

1. Manual installation:

```bash
git clone https://github.com/dwpeng/filterx-c --recursive
cd filterx-c
xmake
```

2. Download the pre-built binary from the [release page](https://github.com/dwpeng/filterx-c/releases)

## Usage

## Concepts

- file: the input file to be filtered.
- filter: use `key=value` or `key` or `-key value` to represent the filter condition.
  - file-based filter: the filter is applied to the file.
  - group-based filter: the filter is applied to the group.
  - process-based filter: the filter is applied to the process.
- record: in one file, a record is one or several rows with the same key.
- group: a group is a set of filters, the first filter will be applied to all files defaultly. Each file can have multiple groups, and the previous group will be applied to the later group.
- process: process records which from the input file, and output the result to the output file based on the file filter condition and process filter condition.

### Filter

filterx supports the following filter conditions:

- `m=[Number]`: means the file contains N-related key-same rows, default is 1.
- `M=[Number]`: means the file contains N-related key-different rows, default is UINT32_MAX.
- `k=[String]`: means choose which column as the key, use a string to represent the column.

  - use a `[number][column_type]` to represent the column, for example, `1s` means the first column is a string type
  - there are 3 column types: `s` means string, `i` means integer, `f` means float
  - the lowercase means the column is sorted from small to large, the uppercase means the column is sorted from large to small

- `[Number]`: means the number of the group-id, `1` means the group-id is 1. The group which id is 1 will be applied to all files defaultly.
- `p=[Char]`: means the placeholder of the output file, each file can have a different placeholder, default is `-`. If the placeholder is `*` or `&`, you need add `\` before it like `\*` or `\&`. Or you can add `" "` to the group filter like `-2 "p=*"`.
- `s=[Char]`: means the separator of the input file, default is `\t`.
- `l=[Number]`: means the limit of every record, for example, `l=10` means the maximum number of rows in each record will be ouputed, if there are more than 10 rows with the same key
- `req=[Y|N]`: means whether the record contains the file. For example, `file:req=N` means only records that do not contain the file will be outputed.
- `c=[Number]`: means the comment line number of the input file, default is #. The comment line will be ignored.
- `cut=[Number]-[Number]`: means the column range of the input file, for example, `cut=1-3` means col1, col2, col3 will be outputed. `cut=3-1` means col3, col2, col1 will be outputed. `cut=` means no column will be outputed.

Except for the above filter conditions, filterx also supports the following filter conditions for the process filter:

- `-cnt [Number],[Number]`: means the number of the group-id and the number of the record-id, for example, `-cnt 1,2` means only records occur at least 1 files and at most 2 files will be outputed, default is 1,UINT32_MAX. `-cnt 1` means only records occur exactly 1 file will be outputed. `-cnt 1,` means only records occur at least 1 file will be outputed. `-cnt ,2` means only records occur at most 2 files will be outputed.
- `-freq [Float],[Float]`: means the frequency of the group-id and the frequency of the record-id, for example, `-freq 0.5,0.8` means only records occur at least 50% files and at most 80% files will be outputed, default is 0.0001,1.0, `-freq 0.5,` means only records occur at least 50% files will be outputed. `-freq ,0.5` means only records occur at most 50% files will be outputed.
- `-L [Number]`: means the limit of the records, for example, `-L 10` means only the top 10 records will be outputed.
- `-s [Char]`: means the separator of the output file, default is `\t`.
- `-o [File]`: means the output file, default is stdout.
- `-R`: row mode, the records will be outputed row by row, default is column mode. Only file's `cut` filter is non-empty, row mode is supported.
- `-F`: full mode, ignore cut parameter, every column will be outputed, but only row mode is supported.

### Group

filterx can combine multiple filters into a group, between each filter, use `:` to separate them.

```bash
-1 "k=1s:m=2"
-2 "cut=1-2:m=2"
```

### File

filterx can add filters to file directly, like group, between each filter, use `:` to separate them.

```bash
file_path:cut=1-2:m=2:p=@:s=,:1:2
```

The above command means the file `file_path` will be applied with the filter `cut=1-2:m=2:p=@:s=,:1:2`.

## Example

### Simple csv example

<details>
<summary>1.csv</summary>

```csv
1,,3,4
1,68,7,8
1,68,7,8
1,68,7,8
1,68,7,8
1,5,6,7
1,4
1,3
1,2,
1,2
1,0
```

</details>

<details>
<summary>2.csv</summary>

```csv
2,,3,4
2,68,7,8
2,68,7,8
2,5,6,7
2,3
2,2,
2,
```

</details>

<details>
<summary>3.csv</summary>

```csv
3,,3,4
3,68,7,8
3,5,6,7
3,4
3,3
3,
```

</details>

```bash
filterx -1 "k=2I:l=1:cut=2:p=$:s=," 1.csv 2.csv 3.csv
```

Output:

```csv
68      68      68
5       5       5
4       $       4
3       3       3
2       2       $
0       $       $
```

```bash
filterx -1 "k=2I:s=," -2 "cut=1:p=#" 1.csv:p=# 2.csv:2 "3.csv:p=&:cut=2"
```

Output:

```csv
68      2       68
68      2       &
68      #       &
68      #       &
5       2       5
4       #       4
3       2       3
2       2       &
2       #       &
0       #       &
```

```bash
filterx -1 "k=2I" -2 "cut=1-3:p=*" 1.csv:p=# 2.csv:2 "3.csv:p=&:2" -cnt 3
```

Output:

```csv
68      2       68      7       3       68      7
68      2       68      7       &       &       &
68      *       *       *       &       &       &
68      *       *       *       &       &       &
5       2       5       6       3       5       6
3       2       3       *       3       3       &
```

```bash
filterx -1 "k=2I" -2 "cut=1-3:p=*" 1.csv:p=# 2.csv:2 "3.csv:p=&:2" -cnt 2
```

Output:

```csv
4       *       *       *       3       4       &
2       2       2       *       &       &       &
2       *       *       *       &       &       &
```

### Query same variant-point in vcf files

<details>
<summary>1.vcf</summary>

```vcf
##fileformat=VCFv4.1
##INFO=<ID=TEST,Number=1,Type=Integer,Description="Testing Tag">
##FORMAT=<ID=TT,Number=A,Type=Integer,Description="Testing Tag, with commas and \"escapes\" and escaped escapes combined with \\\"quotes\\\\\"">
##INFO=<ID=DP4,Number=4,Type=Integer,Description="# high-quality ref-forward bases, ref-reverse, alt-forward and alt-reverse bases">
##FORMAT=<ID=GT,Number=1,Type=String,Description="Genotype">
##FORMAT=<ID=GQ,Number=1,Type=Integer,Description="Genotype Quality">
##FORMAT=<ID=DP,Number=1,Type=Integer,Description="Read Depth">
##FORMAT=<ID=GL,Number=G,Type=Float,Description="Genotype Likelihood">
##FILTER=<ID=q10,Description="Quality below 10">
##FILTER=<ID=test,Description="Testing filter">
##contig=<ID=1,assembly=b37,length=249250621>
##contig=<ID=2,assembly=b37,length=249250621>
##contig=<ID=3,assembly=b37,length=198022430>
##contig=<ID=4,assembly=b37,length=191154276>
##test=<ID=4,IE=5>
##reference=file:///lustre/scratch105/projects/g1k/ref/main_project/human_g1k_v37.fasta
##readme=AAAAAA
##readme=BBBBBB
##INFO=<ID=AC,Number=A,Type=Integer,Description="Allele count in genotypes">
##INFO=<ID=AN,Number=1,Type=Integer,Description="Total number of alleles in called genotypes">
##INFO=<ID=INDEL,Number=0,Type=Flag,Description="Indicates that the variant is an INDEL.">
##INFO=<ID=STR,Number=1,Type=String,Description="Test string type">
#CHROM	POS	ID	REF	ALT	QUAL	FILTER	INFO	FORMAT	A	B
1	3000150	id1	C	T	99	PASS	STR=id1;AN=4;AC=0	GT:GQ	0|0:999	0|0:999
1	3062915	idSNP	G	T,C	99	PASS	STR=testSNP;TEST=5;DP4=1,2,3,4;AN=3;AC=0,0	GT:TT:GQ:DP:GL	0|0:9,9:999:99:-99,-9,-99,-99,-9,-99	0:9,9:999:99:-99,-9,-99
1	3106154	id5	C	CT	99	PASS	STR=id5;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3157410	id6	GA	GC,G	99	PASS	STR=id6;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
1	3162006	id7	GAA	GG	99	PASS	STR=id7;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3177144	id9	G	.	99	PASS	STR=id9;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3184885	id10	TAAAA	TA,T	99	PASS	STR=id10;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
2	3199812	id11	G	GTT,GT	99	PASS	STR=id11;AN=4;AC=0,0	GT:GQ:DP	0|0:999:99	0|0:999:99
3	3212016	id12	CTT	C,CT	99	PASS	STR=id12;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
4	3258448	id13	TACACACAC	T	99	PASS	STR=id13;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
```

</details>

<details>
<summary>2.vcf</summary>

```vcf
##fileformat=VCFv4.1
##INFO=<ID=TEST,Number=1,Type=Integer,Description="Testing Tag">
##FORMAT=<ID=TT,Number=A,Type=Integer,Description="Testing Tag, with commas and \"escapes\" and escaped escapes combined with \\\"quotes\\\\\"">
##INFO=<ID=DP4,Number=4,Type=Integer,Description="# high-quality ref-forward bases, ref-reverse, alt-forward and alt-reverse bases">
##FORMAT=<ID=GT,Number=1,Type=String,Description="Genotype">
##FORMAT=<ID=GQ,Number=1,Type=Integer,Description="Genotype Quality">
##FORMAT=<ID=DP,Number=1,Type=Integer,Description="Read Depth">
##FORMAT=<ID=GL,Number=G,Type=Float,Description="Genotype Likelihood">
##FILTER=<ID=q10,Description="Quality below 10">
##FILTER=<ID=test,Description="Testing filter">
##contig=<ID=1,assembly=b37,length=249250621>
##contig=<ID=2,assembly=b37,length=249250621>
##contig=<ID=3,assembly=b37,length=198022430>
##contig=<ID=4,assembly=b37,length=191154276>
##test=<ID=4,IE=5>
##reference=file:///lustre/scratch105/projects/g1k/ref/main_project/human_g1k_v37.fasta
##readme=AAAAAA
##readme=BBBBBB
##INFO=<ID=AC,Number=A,Type=Integer,Description="Allele count in genotypes">
##INFO=<ID=AN,Number=1,Type=Integer,Description="Total number of alleles in called genotypes">
##INFO=<ID=INDEL,Number=0,Type=Flag,Description="Indicates that the variant is an INDEL.">
##INFO=<ID=STR,Number=1,Type=String,Description="Test string type">
#CHROM	POS	ID	REF	ALT	QUAL	FILTER	INFO	FORMAT	A	B
1	3000150	id1	C	T	99	PASS	STR=id1;AN=4;AC=0	GT:GQ	0|0:999	0|0:999
1	3000151	id2	C	T	99	PASS	STR=id2;AN=4;AC=0	GT:DP:GQ	0|0:99:999	0|0:99:999
1	3062915	idIndel	GTTT	G	99	PASS	DP4=1,2,3,4;AN=4;AC=0;INDEL;STR=testIndel	GT:GQ:DP:GL	0|0:999:99:-99,-9,-99	0|0:999:99:-99,-9,-99
1	3062915	idSNP	G	T,C	99	PASS	STR=testSNP;TEST=5;DP4=1,2,3,4;AN=3;AC=0,0	GT:TT:GQ:DP:GL	0|0:9,9:999:99:-99,-9,-99,-99,-9,-99	0:9,9:999:99:-99,-9,-99
1	3106154	id4	CAAA	C	99	PASS	STR=id4;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3106154	id5	C	CT	99	PASS	STR=id5;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3157410	id6	GA	GC,G	99	PASS	STR=id6;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
1	3162006	id7	GAA	GG	99	PASS	STR=id7;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3177144	id8	G	T	99	PASS	STR=id8;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3177144	id9	G	.	99	PASS	STR=id9;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3184885	id10	TAAAA	TA,T	99	PASS	STR=id10;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
2	3199812	id11	G	GTT,GT	99	PASS	STR=id11;AN=4;AC=0,0	GT:GQ:DP	0|0:999:99	0|0:999:99
3	3212016	id12	CTT	C,CT	99	PASS	STR=id12;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
4	3258448	id13	TACACACAC	T	99	PASS	STR=id13;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
```

</details>

<details>
<summary>3.vcf</summary>

```vcf
##fileformat=VCFv4.1
##INFO=<ID=TEST,Number=1,Type=Integer,Description="Testing Tag">
##FORMAT=<ID=TT,Number=A,Type=Integer,Description="Testing Tag, with commas and \"escapes\" and escaped escapes combined with \\\"quotes\\\\\"">
##INFO=<ID=DP4,Number=4,Type=Integer,Description="# high-quality ref-forward bases, ref-reverse, alt-forward and alt-reverse bases">
##FORMAT=<ID=GT,Number=1,Type=String,Description="Genotype">
##FORMAT=<ID=GQ,Number=1,Type=Integer,Description="Genotype Quality">
##FORMAT=<ID=DP,Number=1,Type=Integer,Description="Read Depth">
##FORMAT=<ID=GL,Number=G,Type=Float,Description="Genotype Likelihood">
##FILTER=<ID=q10,Description="Quality below 10">
##FILTER=<ID=test,Description="Testing filter">
##contig=<ID=1,assembly=b37,length=249250621>
##contig=<ID=2,assembly=b37,length=249250621>
##contig=<ID=3,assembly=b37,length=198022430>
##contig=<ID=4,assembly=b37,length=191154276>
##test=<ID=4,IE=5>
##reference=file:///lustre/scratch105/projects/g1k/ref/main_project/human_g1k_v37.fasta
##readme=AAAAAA
##readme=BBBBBB
##INFO=<ID=AC,Number=A,Type=Integer,Description="Allele count in genotypes">
##INFO=<ID=AN,Number=1,Type=Integer,Description="Total number of alleles in called genotypes">
##INFO=<ID=INDEL,Number=0,Type=Flag,Description="Indicates that the variant is an INDEL.">
##INFO=<ID=STR,Number=1,Type=String,Description="Test string type">
#CHROM	POS	ID	REF	ALT	QUAL	FILTER	INFO	FORMAT	A	B
1	3000150	id1	C	T	99	PASS	STR=id1;AN=4;AC=0	GT:GQ	0|0:999	0|0:999
1	3000151	id2	C	T	99	PASS	STR=id2;AN=4;AC=0	GT:DP:GQ	0|0:99:999	0|0:99:999
1	3062915	idIndel	GTTT	G	99	PASS	DP4=1,2,3,4;AN=4;AC=0;INDEL;STR=testIndel	GT:GQ:DP:GL	0|0:999:99:-99,-9,-99	0|0:999:99:-99,-9,-99
1	3062915	idSNP	G	T,C	99	PASS	STR=testSNP;TEST=5;DP4=1,2,3,4;AN=3;AC=0,0	GT:TT:GQ:DP:GL	0|0:9,9:999:99:-99,-9,-99,-99,-9,-99	0:9,9:999:99:-99,-9,-99
1	3106154	id4	CAAA	C	99	PASS	STR=id4;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3106154	id5	C	CT	99	PASS	STR=id5;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3157410	id6	GA	GC,G	99	PASS	STR=id6;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
1	3162006	id7	GAA	GG	99	PASS	STR=id7;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3177144	id8	G	T	99	PASS	STR=id8;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3177144	id9	G	.	99	PASS	STR=id9;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
1	3184885	id10	TAAAA	TA,T	99	PASS	STR=id10;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
2	3199812	id11	G	GTT,GT	99	PASS	STR=id11;AN=4;AC=0,0	GT:GQ:DP	0|0:999:99	0|0:999:99
3	3212016	id12	CTT	C,CT	99	PASS	STR=id12;AN=4;AC=0,0	GT:GQ:DP	0|0:99:99	0|0:99:99
4	3258448	id13	TACACACAC	T	99	PASS	STR=id13;AN=4;AC=0	GT:GQ:DP	0|0:999:99	0|0:999:99
```

</details>

Find the same variant-point that occurs in at least 2 files.

```bash
filterx -1 "k=1i2i:cut=" 2.vcf:cut=1,2 1.vcf 3.vcf -cnt 2,
```

Output:

```vcf
1       3000150
1       3000151
1       3062915
1       3062915
1       3106154
1       3106154
1       3157410
1       3162006
1       3177144
1       3177144
1       3184885
2       3199812
3       3212016
4       3258448
```

### Cross match between different types of files

<details>
<summary>bed.txt</summary>

```txt
OsR498G0022201900.01.P01
OsR498G0022202400.01.P01
OsR498G0022202500.01.P01
OsR498G0022203500.01.P01
OsR498G0022203900.01.P01
OsR498G0022205200.01.P01
OsR498G0022205900.01.P01
OsR498G0022206400.01.P01
OsR498G0022208400.01.P01
OsR498G0022208500.01.P01
OsR498G0022211300.01.P01
OsR498G0022213200.01.P01
OsR498G0022213900.01.P01
OsR498G0022215000.01.P01
OsR498G0022216400.01.P01
OsR498G0100014500.01.P01
OsR498G0100014700.01.P01
OsR498G0100031800.01.P01
OsR498G0100033300.01.P01
OsR498G0100033600.01.P01
```

</details>

<details>
<summary>blast.txt</summary>

[blast.txt](./test-data/blast-bed/blast.txt)

</details>

Filter the blast result that the query sequence is in the bed file. And output the first blast result of each query sequence.

```bash
filterx -1 "cut=" -2 "k=1s:cut=1:l=1" bed.txt:k=1s blast.txt:2 -cnt 2 -R -F
```

Output:

```txt
OsR498G0022201900.01.P01        OsR498G0022201900.01.P01        100.000 484     0       0       1       484     1       484     0.0     973
OsR498G0022202400.01.P01        OsR498G0022202400.01.P01        100.000 161     0       0       1       161     1       161     3.72e-120       332
OsR498G0022202500.01.P01        OsR498G0022202500.01.P01        100.000 476     0       0       1       476     1       476     0.0     995
OsR498G0022203500.01.P01        OsR498G0022203500.01.P01        100.000 139     0       0       1       139     1       139     6.46e-102       284
OsR498G0022203900.01.P01        OsR498G0022203900.01.P01        100.000 93      0       0       1       93      1       93      1.35e-68        196
OsR498G0022205200.01.P01        OsR498G0022205200.01.P01        100.000 58      0       0       1       58      1       58      1.24e-38        117
OsR498G0022205900.01.P01        OsR498G0022205900.01.P01        100.000 476     0       0       1       476     1       476     0.0     981
OsR498G0022206400.01.P01        OsR498G0022206400.01.P01        100.000 224     0       0       1       224     1       224     7.53e-163       445
OsR498G0022208400.01.P01        OsR498G0022208400.01.P01        100.000 123     0       0       1       123     1       123     1.87e-88        248
OsR498G0022208500.01.P01        OsR498G0022208500.01.P01        100.000 175     0       0       1       175     1       175     8.02e-126       347
OsR498G0022211300.01.P01        OsR498G0022211300.01.P01        100.000 94      0       0       1       94      1       94      4.48e-65        187
OsR498G0022213200.01.P01        OsR498G0022213200.01.P01        100.000 176     0       0       1       176     1       176     8.38e-131       360
OsR498G0022213900.01.P01        OsR498G0022213900.01.P01        100.000 127     0       0       1       127     1       127     5.73e-89        250
OsR498G0022215000.01.P01        OsR498G0022215000.01.P01        100.000 161     0       0       1       161     1       161     3.27e-119       329
OsR498G0022216400.01.P01        OsR498G0022216400.01.P01        100.000 248     0       0       1       248     1       248     0.0     514
OsR498G0100014500.01.P01        OsR498G0100014500.01.P01        100.000 494     0       0       1       494     1       494     0.0     1014
OsR498G0100014700.01.P01        OsR498G0100014700.01.P01        100.000 509     0       0       1       509     1       509     0.0     1043
OsR498G0100031800.01.P01        OsR498G0100031800.01.P01        100.000 1418    0       0       1       1418    1       1418    0.0     2945
OsR498G0100033300.01.P01        OsR498G0100033300.01.P01        100.000 799     0       0       1       799     1       799     0.0     1672
OsR498G0100033600.01.P01        OsR498G0100033600.01.P01        100.000 149     0       0       1       149     1       149     2.12e-103       288
```

Explanation:

- `-1 "cut="`: set all files' output columns to empty, it will be applied to all files defaultly
- `-2 "k=1s:cut=1:l=1"`: use the first column as the key
  - `k=1s`: use the first column as the key
  - `cut=1`: let the output column be non-empty, beacuse group-1 has made the output column empty. Only output columns that are non-empty will be outputed.
  - `l=1` means only output the first record of each key
- `bed.txt:k=1s`: use the first column as the key
- `blast.txt:2`: apply the group-2 filter to the blast file
- `-cnt 2`: only output the records that occur 2 files exactly
- `-R`: row mode, output the records row by row, if the output column is non-empty and use the `-F` parameter, the full mode will be applied
- `-F`: full mode, output all columns

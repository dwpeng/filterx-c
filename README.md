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
- `k=1s2i3I`: means choose the first column as string, the second column as int, and the third column as int. Lowercase means the column is ascending, and uppercase means the column is descending.
- `[Number]`: means the number of the group-id, `1` means the group-id is 1. The group which id is 1 will be applied to all files defaultly.
- `p=[Char]`: means the placeholder of the output file, each file can have a different placeholder, default is `-`. If the placeholder is `*` or `&`, you need add `\` before it like `\*` or `\&`. Or you can add `" "` to the group filter like `-2 "p=*"`.
- `s=[Char]`: means the separator of the input file, default is `\t`.
- `l=[Number]`: means the limit of every record, for example, `l=10` means the maximum number of rows in each record will be ouputed, if there are more than 10 rows with the same key
- `req=[Y|N]`: means whether the record contains the file. For example, `file:req=N` means only records that do not contain the file will be outputed.
- `c=[Number]`: means the comment line number of the input file, default is #. The comment line will be ignored.
- `cut=[Number]-[Number]`: means the column range of the input file, for example, `cut=1-3` means col1, col2, col3 will be outputed. `cut=3-1` means col3, col2, col1 will be outputed. `cut=` means no column will be outputed.

Except for the above filter conditions, filterx also supports the following filter conditions for the process filter:

- `-cnt [Number],[Number]`: means the number of the group-id and the number of the record-id, for example, `-cnt 1,2` means only records occur at least 1 files and at most 2 files will be outputed, default is 1,UINT32_MAX. `-cnt 1` means only records occur exactly 1 file will be outputed.
- `-freq [Float],[Float]`: means the frequency of the group-id and the frequency of the record-id, for example, `-freq 0.5,0.8` means only records occur at least 50% files and at most 80% files will be outputed, default is 0.0001,1.0
- `-L [Number]`: means the limit of the records, for example, `-L 10` means only the top 10 records will be outputed.
- `-s [Char]`: means the separator of the output file, default is `\t`.
- `-o [File]`: means the output file, default is stdout.
- `-R`: row mode, the records will be outputed row by row, default is column mode.
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
filterx -1 "k=1i2i:cut=" 2.vcf:cut=1 1.vcf 3.vcf -cnt 2,
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

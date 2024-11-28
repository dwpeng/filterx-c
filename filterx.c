/*
 *
 * Copyright (c) 2011, Jue Ruan <ruanjue@gmail.com>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <float.h>
#include "string.h"
#include "list.h"
#include "hashset.h"
#include "heap.h"
#include "file_reader.h"

#define KEY_STR_ASC	1
#define KEY_STR_DSC	2
#define KEY_CSTR_ASC	3
#define KEY_CSTR_DSC	4
#define KEY_NUM_ASC	5
#define KEY_NUM_DSC	6
#define KEY_ENUM_ASC	7
#define KEY_ENUM_DSC	8

static int enum_map_idx = 0;
static int chr_enums_cnt = 63;
static char* chr_enums[] = {
	"chr1", "chr2", "chr3", "chr4", "chr5",
	"chr6", "chr7", "chr8", "chr9", "chr10",
	"chr11", "chr12", "chr13", "chr14", "chr15",
	"chr16", "chr17", "chr18", "chr19", "chr20",
	"chr21", "chr22", "chr23", "chr24", "chr25",
	"chr26", "chr27", "chr28", "chr29", "chr30",
	"chr31", "chr32", "chr33", "chr34", "chr35",
	"chr36", "chr37", "chr38", "chr39", "chr40",
	"chr41", "chr42", "chr43", "chr44", "chr45",
	"chr46", "chr47", "chr48", "chr49", "chr50",
	"chr51", "chr52", "chr53", "chr54", "chr55",
	"chr56", "chr57", "chr58", "chr59", "chr60",
	"chrX", "chrY", "chrM"
};

static const char* STR_NULL = "";
#define MAX_CNT	0x7FFFFFFF
typedef struct {
	int colx, coly;
	int cutx, cuty;
	unsigned int grpx, grpy;
	char comm, delim;
	int dup, req, fill;
	int cal_cnt;
	int cnt_min, cnt_max;
	double freq_min, freq_max;
} attr_t;
static const attr_t DEFAULT_ATTR = (attr_t){0, -1, 0, -1, 0, 0, '\0', '\0', 0, -2, -1, 0, 0, MAX_CNT, 0, 1.0};

#define MAX_GROUP	9

typedef struct {
	FileReader *in;
	int has_next;
	u8i select_idx;
	int ncol;
	attr_t attr;
} dat_t;
define_list(dsv, dat_t);

typedef struct {
	int nmemb;
	int cnt;
	attr_t attr;
} grp_t;
define_list(grpv, grp_t);

typedef struct {
	u1v *keys;
	u4v *cols;
	u4v *cuts;
	u4v *gids;
	grp_t grps[MAX_GROUP+1];
	dsv *dats;
	f8v *nvals;
	u4v *evals;
	cplist *svals;
	u4v *heap;
	cuhash *emaps;
	FILE *out;
} FILTERX;

FILTERX* init_filterx(){
	FILTERX *x;
	grp_t *g;
	int i;
	x = calloc(sizeof(FILTERX), 1);
	x->keys = init_u1v(32);
	x->cols = init_u4v(32);
	x->cuts = init_u4v(32);
	x->gids = init_u4v(32);
	for(i=0;i<=MAX_GROUP;i++){
		g = x->grps + i;
		g->nmemb = 0;
		g->cnt = 0;
		g->attr = DEFAULT_ATTR;
		g->attr.comm = '#';
		g->attr.delim = '\t';
		g->attr.req = 1;
		g->attr.fill = 0;
	}
	
	x->dats = init_dsv(32);
	x->nvals = init_f8v(32);
	x->evals = init_u4v(32);
	x->svals = init_cplist(32);
	x->heap = init_u4v(32);
	x->emaps = init_cuhash(1023);
	x->out = stdout;
	return x;
}

void free_filterx(FILTERX *x){
	//int i;
	free_u1v(x->keys);
	free_u4v(x->cols);
	free_u4v(x->cuts);
	free_dsv(x->dats);
	free_f8v(x->nvals);
	free_u4v(x->evals);
	free_cplist(x->svals);
	free_u4v(x->heap);
	//free_cplist(x->estrs);
	free_u4v(x->gids);
	free_cuhash(x->emaps);
	free(x);
}

void builtin_enums_filterx(FILTERX *x){
	int i;
	for(i=0;i<chr_enums_cnt;i++){
		kv_put_cuhash(x->emaps, chr_enums[i], ++enum_map_idx);
	}
}

// par should be referenced before program exit
void parse_enums_filterx(FILTERX *x, char *par){
	char *str, *tok;
	str = par;
	do {
		tok = index(str, ','); if(tok) *tok = '\0';
		kv_put_cuhash(x->emaps, str, ++enum_map_idx);
		str = tok + 1;
	} while(tok);
}

void parse_keys_filterx(FILTERX *x, char *par){
	char *p;
	u4i i;
	int key;
	clear_u1v(x->keys);
	p = par;
	while(*p){
		key = 0;
		switch(*p){
			case 's': key = KEY_STR_ASC; break;
			case 'S': key = KEY_STR_DSC; break;
			case 'c': key = KEY_CSTR_ASC; break;
			case 'C': key = KEY_CSTR_DSC; break;
			case 'n': key = KEY_NUM_ASC; break;
			case 'N': key = KEY_NUM_DSC; break;
			case 'e': key = KEY_ENUM_ASC; break;
			case 'E': key = KEY_ENUM_DSC; break;
			default: fprintf(stderr, "Unknown key type '%c', abort!\n", *p); exit(1);
		}
		push_u1v(x->keys, key);
		p ++;
	}
	clear_u4v(x->cols);
	for(i=0;i<x->keys->size;i++){
		push_u4v(x->cols, i + 1);
	}
	for(i=0;i<=MAX_GROUP;i++){
		x->grps[i].attr.colx = 0;
		x->grps[i].attr.coly = x->keys->size;
	}
}

void parse_attr_filterx(FILTERX *x, attr_t *attr, char *par){
	int kcol, xcol, ycol, grp;
	int ncol, i;
	char *str, *tok, *ttk, *ttt;
	str = par;
	ncol = 0;
	kcol = 0;
	while(str && *str){
		tok = index(str, ':'); if(tok) *tok = '\0';
		if(str[0] >= '0' && str[0] <= '9'){
			if(ncol == 0) attr->colx = x->cols->size;
			kcol = atoi(str);
			if(ncol < (int)x->keys->size){ push_u4v(x->cols, kcol); attr->coly = x->cols->size; }
			ncol ++;
		} else if(strncasecmp(str, "grp", 3) == 0 && str[3] == '=' && str[4] >= '1' && str[4] <= '9'){
			str = str + 4;
			do {
				ttk = index(str, ','); if(ttk) *ttk = '\0';
				grp = atoi(str);
				if(grp >= 1 && grp <= MAX_GROUP){
					if(attr->grpy == attr->grpx) attr->grpx = x->gids->size;
					push_u4v(x->gids, grp);
					attr->grpy = x->gids->size;
					//x->grps[grp].nmemb ++;
				} else {
					fprintf(stderr, "Error in parsing tag 'grp' %d\n", grp); exit(1);
				}
				str=ttk+1;
			} while(ttk);
		} else if(strncasecmp(str, "dup", 3) == 0 && str[3] == '=' && (uc(str[4]) == 'Y' || uc(str[4]) == 'N')){
			attr->dup = (uc(str[4]) == 'Y');
		} else if(strncasecmp(str, "fill", 4) == 0 && str[4] == '=' && (uc(str[5]) == 'Y' || uc(str[5]) == 'N')){
			attr->fill = (uc(str[5]) == 'Y');
		} else if(strncasecmp(str, "delim", 5) == 0 && str[5] == '='){
			if(str[6]){
				if(str[6] == '\\'){
					if(str[7]){
						if(str[7] == 't'){
							attr->delim = '\t';
						}
					}
				} else {
					attr->delim = str[6];
				}
			}
		} else if(strncasecmp(str, "comm", 4) == 0 && str[4] == '='){
			attr->comm = str[5];
		} else if(strncasecmp(str, "req", 3) == 0 && str[3] == '=' && (uc(str[4]) == 'Y' || uc(str[4]) == 'N' || uc(str[4]) == 'E')){
			switch(uc(str[4])){
				case 'Y': attr->req = 1; break;
				case 'N': attr->req = 0; break;
				case 'E': attr->req = -1; break;
			}
		} else if(strncasecmp(str, "cut", 3) == 0 && str[3] == '=' && ((str[4] >= '0' && str[4] <= '9') || str[4] == '\0')){
			if(str[4] == '\0'){
				attr->cuty = attr->cutx = 0;
			} else {
				str = str + 4;
				do {
					ttk = index(str, ','); if(ttk) *ttk = '\0';
					ttt = index(str, '-');
					if(ttt == NULL){
						xcol = atoi(str);
						if(xcol >= 1){
							if(attr->cuty == -1) attr->cutx = x->cuts->size;
							push_u4v(x->cuts, xcol);
							attr->cuty = x->cuts->size;
						} else {
							fprintf(stderr, "Error in parsing tag 'cut'\n"); exit(1);
						}
					} else {
						*ttt = '\0';
						xcol = atoi(str);
						ycol = atoi(ttt + 1);
						if(xcol >= 1 && ycol >= kcol){
							if(attr->cuty == -1) attr->cutx = x->cuts->size;
							for(i=xcol;i<=ycol;i++) push_u4v(x->cuts, i);
							attr->cuty = x->cuts->size;
						} else {
							fprintf(stderr, "Error in parsing tag 'cut'\n"); exit(1);
						}
					}
					str=ttk+1;
				} while(ttk);
			}
		} else if(strncasecmp(str, "cnt", 3) == 0){
			attr->cal_cnt = 1;
			if(str[3] == '='){
				attr->cnt_min = attr->cnt_max = atoi(str + 4);
			} else if(str[3] == '>'){
				if(str[4] == '='){
					attr->cnt_min = atoi(str + 5);
				} else {
					attr->cnt_min = atoi(str + 4) + 1;
				}
			} else if(str[3] == '<'){
				if(str[4] == '='){
					attr->cnt_max = atoi(str + 5);
				} else {
					attr->cnt_max = atoi(str + 4) - 1;
				}
			}
		} else if(strncasecmp(str, "freq", 4) == 0){
			attr->cal_cnt = 1;
			if(str[4] == '='){
				attr->freq_min = attr->freq_max = atof(str + 5);
			} else if(str[4] == '>'){
				if(str[5] == '='){
					attr->freq_min = atof(str + 6);
				} else {
					attr->freq_min = atof(str + 5) + FLT_MIN;
				}
			} else if(str[4] == '<'){
				if(str[5] == '='){
					attr->freq_max = atof(str + 6);
				} else {
					attr->freq_max = atof(str + 5) - FLT_MIN;
				}
			}
		} else {
			fprintf(stderr, "Cannot parse '%s'\n", str); exit(1);
		}
		if(tok) str = tok + 1;
		else break;
	}
	/*
	if(ncol == 0) attr->colx = x->cols->size;
	while(ncol++ < (int)x->keys->size){
		push_u4v(x->cols, ++kcol);
	}
	attr->coly = x->cols->size;
	if(attr->grpx == attr->grpy){
		attr->grpx = x->gids->size;
		push_u4v(x->gids, 1);
		d->grpy = x->gids->size;
		//x->grps[1].nmemb ++;
	}
	*/
}

void parse_group_filterx(FILTERX *x, int grp, char *par){
	grp_t *g;
	int kcol, ncol;
	if(grp > MAX_GROUP){
		fprintf(stderr, "Error: exceed MAX_GROUP(%d)\n", MAX_GROUP); exit(1);
	}
	g = x->grps + grp;
	parse_attr_filterx(x, &g->attr, par);
	if(g->attr.coly <= g->attr.colx){ kcol = 0; ncol = 0; }
	else { ncol = g->attr.coly - g->attr.colx; kcol = x->cols->buffer[g->attr.coly - 1]; }
	while(ncol ++ < (int)x->keys->size) push_u4v(x->cols, ++kcol);
	g->attr.coly = x->cols->size;
	g->attr.grpx = g->attr.grpy = 0;
	if(g->attr.cal_cnt) g->attr.req = -1;
}

void parse_file_filterx(FILTERX *x, char *par){
	grp_t *g;
	dat_t *d;
	int kcol, grp;
	int ncol;
	u4i i;
	char *str, *tok;
	str = par;
	d = next_ref_dsv(x->dats);
	d->has_next = 1;
	d->select_idx = 0;
	d->ncol = 0;
	d->attr = DEFAULT_ATTR;
	kcol = 0;
	for(i=0;i<x->keys->size;i++){
		push_f8v(x->nvals, 0);
		push_u4v(x->evals, 0);
		push_cplist(x->svals,(char*)STR_NULL);
	}
	tok = index(str, ':'); if(tok) *tok = '\0';
	d->in = fopen_filereader(str);
	if(tok) parse_attr_filterx(x, &d->attr, tok + 1);
	// grp
	if(d->attr.grpy <= d->attr.grpx){
		grp = 1;
		d->attr.grpx = x->gids->size;
		push_u4v(x->gids, grp);
		d->attr.grpy = x->gids->size;

	} else {
		grp = x->gids->buffer[d->attr.grpx];
	}
	for(i=d->attr.grpx;i<d->attr.grpy;i++) x->grps[x->gids->buffer[i]].nmemb ++;
	g = x->grps + grp;
	//colx, coly
	if(d->attr.coly <= d->attr.colx){
		d->attr.colx = g->attr.colx;
		d->attr.coly = g->attr.coly;
	} else {
		ncol = d->attr.coly - d->attr.colx;
		kcol = x->cols->buffer[d->attr.coly - 1]; 
		while(ncol ++ < (int)x->keys->size) push_u4v(x->cols, ++kcol);
		d->attr.coly = x->cols->size;
	}
	// cutx, cuty
	if(d->attr.cuty < d->attr.cutx){
		d->attr.cutx = g->attr.cutx;
		d->attr.cuty = g->attr.cuty;
	}
	if(d->attr.comm == 0) d->attr.comm = g->attr.comm;
	if(d->attr.delim == 0) d->attr.delim = g->attr.delim;
	d->in->delimiter = d->attr.delim;
	if(d->attr.req == -2) d->attr.req = g->attr.req;
	if(d->attr.fill == -1) d->attr.fill = g->attr.fill;
}

int cmp_two_records_filterx(FILTERX *x, int idx1, int idx2){
	int i, cmp;
	double v1, v2;
	u4i e1, e2;
	for(i=0;i<(int)x->keys->size;i++){
		switch(x->keys->buffer[i]){
			case KEY_STR_ASC: if((cmp = strcmp(x->svals->buffer[idx1 * x->keys->size + i], x->svals->buffer[idx2 * x->keys->size + i]))) return cmp; break;
			case KEY_STR_DSC: if((cmp = strcmp(x->svals->buffer[idx2 * x->keys->size + i], x->svals->buffer[idx1 * x->keys->size + i]))) return cmp; break;
			case KEY_CSTR_ASC: if((cmp = strcasecmp(x->svals->buffer[idx1 * x->keys->size + i], x->svals->buffer[idx2 * x->keys->size + i]))) return cmp; break;
			case KEY_CSTR_DSC: if((cmp = strcasecmp(x->svals->buffer[idx2 * x->keys->size + i], x->svals->buffer[idx1 * x->keys->size + i]))) return cmp; break;
			case KEY_NUM_ASC:
				v1 = x->nvals->buffer[idx1 * x->keys->size + i];
				v2 = x->nvals->buffer[idx2 * x->keys->size + i];
				if(v1 < v2) return -1;
				else if(v1 > v2) return 1;
				break;
			case KEY_NUM_DSC:
				v1 = x->nvals->buffer[idx2 * x->keys->size + i];
				v2 = x->nvals->buffer[idx1 * x->keys->size + i];
				if(v1 < v2) return -1;
				else if(v1 > v2) return 1;
				break;
			case KEY_ENUM_ASC:
				e1 =x->evals->buffer[idx1 * x->keys->size + i];
				e2 =x->evals->buffer[idx2 * x->keys->size + i] ;
				if(e1 < e2) return -1;
				else if(e1 > e2) return 1;
				break;
			case KEY_ENUM_DSC:
				e1 =x->evals->buffer[idx2 * x->keys->size + i];
				e2 =x->evals->buffer[idx1 * x->keys->size + i];
				if(e1 < e2) return -1;
				else if(e1 > e2) return 1;
				break;
		}
	}
	return 0;
}

int read_next_record_filterx(FILTERX *x, int i){
	dat_t *dat;
	u4i j;
	int ncol, kcol, kidx;
	dat = ref_dsv(x->dats, i);
	if(dat->has_next == 0) return -1;
	do {
		ncol = fread_table(dat->in);
		if(ncol == -1){ fclose_filereader(dat->in); dat->has_next = 0; return -1; }
		if(dat->in->line->string[0] == dat->attr.comm) continue;
		if(ncol == 0) continue;
		if(dat->ncol == 0) dat->ncol = ncol;
		//dat->req_next = 0;
		for(j=0;j<x->keys->size;j++){
			kidx = i * x->keys->size + j;
			kcol = x->cols->buffer[dat->attr.colx + j];
			if(kcol > ncol){
				fprintf(stderr, "Runtime error: key column %d > real columns %d of %dth file\n", kcol, ncol, i + 1); exit(1);
			}
			switch(x->keys->buffer[j]){
				case KEY_STR_ASC:
				case KEY_STR_DSC:
				case KEY_CSTR_ASC:
				case KEY_CSTR_DSC:
					x->svals->buffer[kidx] = get_col_str(dat->in, kcol-1); break;
				case KEY_NUM_ASC:
				case KEY_NUM_DSC:
					x->nvals->buffer[kidx] = atof(get_col_str(dat->in, kcol-1)); break;
				case KEY_ENUM_ASC:
				case KEY_ENUM_DSC:
					x->evals->buffer[kidx] =kv_get_cuhash(x->emaps, get_col_str(dat->in, kcol-1)); break;
			}
		}
		return ncol;
	} while(1);
}

u8i run_filterx(FILTERX *x){
	dat_t *dat, *ref;
	grp_t *grp;
	cplist *fillstrs;
	u4v *idxs, *gids;
	u8i ret, round;
	u4i i, j, idx;
	int ncol, col, dup;
	int pass ;
	idxs = init_u4v(32);
	gids = init_u4v(10);
	fillstrs = init_cplist(32);
	ret = 0;
	for(i=1;i<=MAX_GROUP;i++){
		if(x->grps[i].attr.cal_cnt == 0 || x->grps[i].nmemb == 0) continue;
		if(x->grps[i].attr.cnt_min < x->grps[i].attr.freq_min * x->grps[i].nmemb) x->grps[i].attr.cnt_min = ceil(x->grps[i].attr.freq_min * x->grps[i].nmemb);
		if(x->grps[i].attr.cnt_max > x->grps[i].attr.freq_max * x->grps[i].nmemb) x->grps[i].attr.cnt_max = floor(x->grps[i].attr.freq_max * x->grps[i].nmemb);
		push_u4v(gids, i);
	}
	dup = 0;
	for(i=0;i<x->dats->size;i++) if(x->dats->buffer[i].attr.dup) dup++;
	if(dup > 1){
		fprintf(stderr, "Error: Tag 'dup' only can be set to 'Y' in one file, but %d files\n", dup); exit(1);
	}
	// init heap-merging
	clear_u4v(x->heap);
	for(i=0;i<x->dats->size;i++){
		ncol = read_next_record_filterx(x, i);
		if(ncol > 0){
			array_heap_push(x->heap->buffer, x->heap->size, x->heap->cap, u4i, i, cmp_two_records_filterx(x, a, b));
			while(fillstrs->size <(unsigned int)ncol) push_cplist(fillstrs, "-");
		}
	}
	// heap-merging
	round = x->dats->size;
	
	do {
		idx = array_heap_pop(x->heap->buffer, x->heap->size, x->heap->cap, u4i, cmp_two_records_filterx(x, a, b));
		if(idx == 0xFFFFFFFFU) break;
		push_u4v(idxs, idx);
		while(1){
			/*make sure idxs not empty*/
			if(idxs->size==0) break;
			/*if the top element of the heap is equal to any element in idxs,then move it to idxs*/
			idx = (x->heap->size)?x->heap->buffer[0]:0xFFFFFFFFU;
			if((idx != 0xFFFFFFFFU)&&(cmp_two_records_filterx(x, idxs->buffer[0], idx)==0)) {
				idx = array_heap_pop(x->heap->buffer, x->heap->size, x->heap->cap, u4i, cmp_two_records_filterx(x, a, b));
				push_u4v(idxs, idx);
				continue;
			}  

			/*when heap is empty or a different value with elements in idxs is detected*/	
			for(i=0;i<gids->size;i++){
				grp = x->grps + gids->buffer[i];
				grp->cnt = 0;
			}
			dup  = -1;
			for(i=0;i<idxs->size;i++){
				dat = ref_dsv(x->dats, idxs->buffer[i]);
				dat->select_idx = round + i;
				for(j=dat->attr.grpx;j<dat->attr.grpy;j++){
					x->grps[x->gids->buffer[j]].cnt ++;
				}
				if(dat->attr.dup) dup = i;
			}
			// filtering
			pass = 1;

			// 1, req
			for(i=0;i<x->dats->size;i++){
				dat = ref_dsv(x->dats, i);
				if(dat->attr.req == 1){
					if(dat->select_idx < round){
						pass = 0;
						break;
					}
				} else if(dat->attr.req == 0){
					if(dat->select_idx >= round){
						pass = 0;
						break;
					}
				}
			}
	
			// 2, grp cnt
			for(i=0;i<gids->size;i++){
				grp = x->grps + gids->buffer[i];
				if(grp->cnt < grp->attr.cnt_min || grp->cnt > grp->attr.cnt_max){
					pass = 0;
					break;
				}
			}
	
			// output
			if(pass){
				for(i=0;i<x->dats->size;i++){
					if(i) fputc('\t', x->out);
					dat = ref_dsv(x->dats, i);
					if(dat->select_idx < round){
						if(dat->attr.fill){
							ref = ref_dsv(x->dats, idxs->buffer[0]);
							for(j=0;j<x->keys->size;j++){
								set_cplist(fillstrs, x->cols->buffer[dat->attr.colx + j]-1, get_col_str(ref->in, x->cols->buffer[ref->attr.colx + j]-1));
							}
						}
							
						if(dat->attr.cuty < dat->attr.cutx) {
							for(col=0;col<dat->ncol;col++){
								if(col) fputc('\t', x->out);
								fprintf(x->out, "%s", get_cplist(fillstrs, col));
								set_cplist(fillstrs, col, "-");
							}
						}
						else {
							col = 0;
							for(j=(unsigned int)dat->attr.cutx;j<(unsigned int)dat->attr.cuty;j++){
								if(col) fputc('\t', x->out);
								col = x->cuts->buffer[j];
								if(col<= dat->ncol ){
									fprintf(x->out, "%s", get_cplist(fillstrs, col - 1));
								}
							}
								
						}

					} else if(dat->attr.cuty < dat->attr.cutx){
						for(j=0;j<dat->in->tabs->size;j++){
							if(j) fputc('\t', x->out);
							fprintf(x->out, "%s", get_col_str(dat->in, j));
						}
					} else {
						col = 0;
						for(j=(unsigned int)dat->attr.cutx;j<(unsigned int)dat->attr.cuty;j++){
							if(col) fputc('\t', x->out);
							col = x->cuts->buffer[j];
							if(col  > (int)dat->in->tabs->size){
								fputc('-', x->out);
							} else {
								fprintf(x->out, "%s", get_col_str(dat->in, col - 1));
							}
						}
					}
				}
				fputc('\n', x->out);
				ret ++;
			}
			// preparing next
			if(dup >= 0){
				for(i=0;i<idxs->size;i++){
					if(i == (unsigned int)dup){
						// update
						ncol = read_next_record_filterx(x, idxs->buffer[i]);
						if(ncol > 0){
							array_heap_push(x->heap->buffer, x->heap->size, x->heap->cap, u4i, idxs->buffer[i], cmp_two_records_filterx(x, a, b));
						}
					} else {
						// push the same content
						array_heap_push(x->heap->buffer, x->heap->size, x->heap->cap, u4i, idxs->buffer[i], cmp_two_records_filterx(x, a, b));
					}
				}
			} else {
				for(i=0;i<idxs->size;i++){
					ncol = read_next_record_filterx(x, idxs->buffer[i]);
					if(ncol > 0){
						array_heap_push(x->heap->buffer, x->heap->size, x->heap->cap, u4i, idxs->buffer[i], cmp_two_records_filterx(x, a, b));
					}
				}
			}
			clear_u4v(idxs);

			if(idx != 0xFFFFFFFFU) {
				idx = array_heap_pop(x->heap->buffer, x->heap->size, x->heap->cap, u4i, cmp_two_records_filterx(x, a, b));
				push_u4v(idxs, idx);
			} else {
				if(x->heap->size==0) break;
				idx = array_heap_pop(x->heap->buffer, x->heap->size, x->heap->cap, u4i, cmp_two_records_filterx(x, a, b));
				push_u4v(idxs, idx);
			}		
			round += x->dats->size;
		}
	} while(0);
	free_u4v(gids);
	free_u4v(idxs);
	free_cplist(fillstrs);
	return ret;
}

int usage(){
	printf(
	"FILTERX: combine multiple table-like files, and output them as user specified\n"
	"Input files must be sorted according to key_types. Unexpected behaviors on unsorted files, except only one file as input\n"
	"Usage: filterx [options] <file_name:attribute> ...\n"
	"Options:\n"
        "Options order: -k, -b, -e, -[1-9]\n"
	" -k     <string>  A string of key_types, values may take from {'s', 'S', 'c', 'C', 'n', 'N', 'e', 'E'}\n"
	"                 's','S': dictionary-order-sort, case-sensitive, 's' for ascending and 'S' for descending\n"
	"                 'c','c': dictionary-order-sort, case-insensitive, 'c' for ascending and 'S' for descending\n"
	"                 'n','N': general-numeric-sort, 'n' for ascending and 'N' for descending\n"
	"                 'e','E': enumeration-sort, 'e' for ascending and 'E' for descending\n"
	"                          enum value can be specified as user defined through '-e' or built-in through '-b' option \n"
	"                  consecutive characters for multiple key_type,e.g. 'sn' indicates files sorted by dictionary-order  and\n"
	"                  general-numeric-order.\n"
	" -b               Built-in enums for chromosomes\n"
	"                  values may take from {chr1,chr2,...chr60,chrX,chrY,chrM}\n"
	" -e     <string>  A list of string to define enums,delimited by ','\n"
	"                  e.g. 'red,green,yellow' means red < green < yellow < undefined\n"
	" -[1-9] <string>  Attributes for group 1..9, delimited by ':' \n"
	"                  -1 for group 1 and -2..9 for group 2 to 9.\n"
	"                  Group attributes may be count or freqency of the occurrence,output columns etc.Detail as follows.\n"
	" -o     <string>  Output file name. Default is stdout.\n"
  "\n"
	"Attribute:\n"
	"Group attributes and file attributes are provided for a flexible way to adjust the output according to user specified.\n"
	"An input file can belong to one or more groups,In default,it belongs to group 1.\n"
	"Attributes of a file will be extended from its  first group. Attributes  specified for file will  overwrite its group\n"
	"attributes. e.g. If file1 belongs to group 2 and group3,file1 will have group attributes of group 2 and its specified\n"
	"file attributes.File attributes are preferred when conflict with group properties.\n"
	"\n"
	"List of the attributes:\n"
	"  number:           column for key_type, '-k sne file1:1:2:3' means the 1st col is 's', 2nd is 'n', 3rd is 'e'\n"
	"  grp=[1-9]:        file attr.  'grp='2,3' means  it belongs  to  group 2 and group 3.  Attributes  of a file will be\n"
	"                    inherited from its first group:group 1.In default, file's group is 1.\n"
	"  req=[Y|N|E]:      requirement, Y: MUST exist, N: MUST NOT, E: either\n"
	"  fill=[Y|N]:       if absent, whether fill the key cols with existences\n"
	"  dup=[Y|N]:        file attr. only one file can have duplicated records\n"
	"  cut=16,2,5-7:     output col16\\tcol2\\tcol5\\tcol6\\tcol7. In default, output all cols\n"
	"  cut=:             don't output any cols for this file\n"
	"  delim=\\t:         means delimiter is '\\t'\n"
	"  comm=#:           lines started with # will be skiped\n"
	"  cnt[op][int]:     group attr, op: >, >=, =, <, <= 'cnt>=2' means at least two files occur in the group.\n"
	"  freq[op][float]:  group attr,same with 'cnt'. 'freq<0.1' means less than 10%% of the input files occur  in the group.\n"
	"\n"
	"Example 1:\n"
	"    filterx -k en -b human1.pos.txt:1:2:req=Y human2.pos.txt:cut= >shared.pos.txt\n"
	"Example 2:\n"
	"    filterx -k s -1 'cnt>1:freq<=0.1:cut=2:fill=Y' s1.txt:cut=1,2 s2.txt s3.txt:req=N ... s100.txt\n"
	"Example 3:\n"
	"    filterx -k n -2 'cnt=1' file1:req=Y file2:grp=2 file3:grp=2\n"
	"    filterx -k n -1 'cnt=2'  -2 'cnt=1' file1:grp=1 file2:grp=1,2 file3:grp=1,2\n"
	"    # record cannot exist in both file2 and file3, but need to exist in file1\n"
	"Example 4:\n"
	"    filterx -k s file1:cut=3,2,1\n"
	"    # output col3\\tcol2\\tcol1, file1 can be un-sorted. Different with linux `cut`\n"
	);
	return 1;
}

int main(int argc, char **argv){
	FILTERX *x;
	int c;
	x = init_filterx();
	while((c = getopt(argc, argv, "hk:be:1:2:3:4:5:6:7:8:9:")) != -1){
		switch(c){
			case 'h': {usage();free_filterx(x);return 0;}
			case 'k': parse_keys_filterx(x, optarg); break;
			case 'b': builtin_enums_filterx(x); break;
			case 'e': parse_enums_filterx(x, optarg); break;
			case 'o': x->out = fopen(optarg, "w"); break;
			default:
			if(c >= '1' && c <= '9'){
				parse_group_filterx(x, c - '0', optarg);
			} else {
				fprintf(stderr, "Unknown option -%c\n", c); exit(1);
			}
		}
	}
	if(optind == argc) {usage();free_filterx(x);return 1;}
	for(c=optind;c<argc;c++) parse_file_filterx(x, argv[c]);
	run_filterx(x);
	if(x->out != stdout) fclose(x->out);
	free_filterx(x);
	return 0;
}

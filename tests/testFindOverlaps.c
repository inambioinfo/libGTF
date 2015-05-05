#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include "../gtf.h"
#include "../htslib/htslib/sam.h"

void findOverlapsBAM2(GTFtree *t, htsFile *fp, bam_hdr_t *hdr, int matchType, int strandType, int minMapq) {
    bam1_t *b = bam_init1();
    overlapSet *os = os_init(t);
    kstring_t *ks = calloc(1, sizeof(kstring_t));
    int32_t i, start, end;
    uint32_t *CIGAR, op;
    assert(b);
    assert(os);
    assert(ks);

    while(sam_read1(fp, hdr, b) >= 0) {
        if(b->core.tid < 0) continue;
        if(b->core.flag & BAM_FUNMAP) continue;
        if(b->core.qual < minMapq) continue;

        //Iterate over mapped segments, finding overlaps of each mapped segment
        start = b->core.pos;
        end = b->core.pos-1;
        CIGAR = bam_get_cigar(b);
        for(i=0; i<b->core.n_cigar; i++) {
            op = bam_cigar_op(CIGAR[i]);
            if(bam_cigar_type(op) == 3) { //M, = or X
                end += bam_cigar_oplen(CIGAR[i]);
            } else if(bam_cigar_type(op) == 2) { //D or N
                if(end >= start) {
                    os = findOverlaps(os,
                          t,
                          hdr->target_name[b->core.tid],
                          start,
                          end+1,
                          (b->core.flag&16)?1:0,
                          matchType,
                          strandType,
                          1); //Note that we must run os_reset() as the end!
                }
                start = end + bam_cigar_oplen(CIGAR[i]) + 1;
                end = start-1;
            }
        }
        if(end >= start) {
            os = findOverlaps(os,
                t,
                hdr->target_name[b->core.tid],
                start,
                end+1,
                (b->core.flag&16)?1:0,
                matchType,
                strandType,
                1);
        }

        if(os->l) {
            if(cntGeneIDs(os) == 1) {
                printf("%s\t%s\n", bam_get_qname(b), GTFgetGeneID(t, os->overlaps[0]));
            } else {
                printf("%s\tAmbiguous\n", bam_get_qname(b));
            }
        } else {
            printf("%s\tUnassigned_NoFeatures\n", bam_get_qname(b));
        }
        os_reset(os);
    }
    bam_destroy1(b);
    os_destroy(os);
    free(ks->s);
    free(ks);
}

//Filter out everything **except** exon/gene/CDS/transcript entries
int exon(void *l) {
    GTFline *line = (GTFline *) l;
    if(strcmp(line->feature.s, "exon") == 0) return 1;
    return 0;
}
int gene(void *l) {
    GTFline *line = (GTFline *) l;
    if(strcmp(line->feature.s, "gene") == 0) return 1;
    return 0;
}
int CDS(void *l) {
    GTFline *line = (GTFline *) l;
    if(strcmp(line->feature.s, "CDS") == 0) return 1;
    return 0;
}
int transcript(void *l) {
    GTFline *line = (GTFline *) l;
    if(strcmp(line->feature.s, "transcript") == 0) return 1;
    return 0;
}

void usage() {
    fprintf(stderr, "Usage: testFindOverlaps [OPTIONS] <annotation.gtf> <alignments.bam>\n");
    fprintf(stderr, "\n"
"This program demostrates how to use libGTF to find the overlaps of each\n"
"alignment in a BAM file. Currently, paired-end reads aren't treated in a\n"
"particularly useful way. It does, however, handle spliced alignments.\n"
"\n"
"N.B., this program also demonstrates how a filter function works, so only\n"
"entries marked as \"exon\" are used!\n"
"\nOPTIONS\n"
"-m STR  Match type. Possible values are 'any', 'exact', 'contain', 'within',\n"
"        'start' and 'end'. These values are equivalent to the 'type' parameter\n"
"        in the findOverlaps() function in GenomicRanges and also Allen's\n"
"        Interval Algebra. 'contain' is simply the opposite of 'within'. The\n"
"        default is 'any'.\n"
"-s STR  Strand type. Possible values are 'ignore', 'same', 'opposite', and\n"
"        'exact'. 'exact' differs from 'same' in how '*' strands are handled.\n"
"        Normally, a subject and query will overlaps if either of them has a '*'\n"
"        strand. The 'exact' option indicates that strands must exactly match.\n"
"-q INT  Minimum MAPQ value. Default is [0].\n"
"-t INT  Entry type to include (according to the 'feature' column). 0: all\n"
"        entries, 1: exon, 2: gene, 3: CDS, 4: transcript. Default is 0.\n"
);
}

int main(int argc, char *argv[]) {
    int matchType = 0;
    int strandType = 0;
    int minMapq = 0;
    int type = 0;
    char c;
    htsFile *fp = NULL;
    bam_hdr_t *hdr = NULL;
    GTFtree *t = NULL;

    opterr = 0; //Disable error messages
    while((c = getopt(argc, argv, "m:s:q:ht:")) >= 0) {
        switch(c) {
        case 'm' :
            if(strcmp(optarg, "any") == 0) matchType = 0;
            else if(strcmp(optarg, "exact") == 0) matchType = 1;
            else if(strcmp(optarg, "contain") == 0) matchType = 2;
            else if(strcmp(optarg, "within") == 0) matchType = 3;
            else if(strcmp(optarg, "start") == 0) matchType = 4;
            else if(strcmp(optarg, "end") == 0) matchType = 5;
            else fprintf(stderr, "Unknown -m option '%s', ignoring\n", optarg);
            break;
        case 's' :
            if(strcmp(optarg, "ignore") == 0) strandType = 0;
            else if(strcmp(optarg, "same") == 0) strandType = 1;
            else if(strcmp(optarg, "opposite") == 0) strandType = 2;
            else if(strcmp(optarg, "exact") == 0) strandType = 3;
            else fprintf(stderr, "Unknown -s option '%s', ignoring\n", optarg);
            break;
        case 't' :
            type = atoi(optarg);
            if(type<0 || type > 4) {
                fprintf(stderr, "Error: -t must be between 0 and 4!\n");
                return 1;
            }
            break;
        case 'h' :
            usage();
            return 0;
            break;
        case 'q' :
            minMapq = atoi(optarg);
            if(minMapq < 0) minMapq = 0;
            break;
        case '?' :
        default :
            fprintf(stderr, "Invalid option '%s'\n", argv[optind-1]);
            usage();
            return 1;
            break;
        }
    }

    if(argc == 1) {
        usage();
        return 0;
    }
    if(argc-optind != 2) {
        fprintf(stderr, "Missing either the GTF or BAM file!\n");
        usage();
        return 1;
    }

    //Create the GTFtree
    if(!type) t = GTF2Tree(argv[optind], NULL);
    else if(type==1) t = GTF2Tree(argv[optind], exon);
    else if(type==2) t = GTF2Tree(argv[optind], gene);
    else if(type==3) t = GTF2Tree(argv[optind], CDS);
    else if(type==4) t = GTF2Tree(argv[optind], transcript);
    if(!t) {
        fprintf(stderr, "Couldn't open %s or there was a problem parsing it.\n", argv[1]);
        return 1;
    }
    sortGTF(t);

    //Open the BAM file
    fp = sam_open(argv[optind+1], "r");
    if(!fp) {
        fprintf(stderr, "Couldn't open %s for reading!\n", argv[optind+1]);
        destroyGTFtree(t);
    }
    hdr = sam_hdr_read(fp);
    if(!hdr) {
        fprintf(stderr, "Couldn't read the header from %s!\n", argv[optind+1]);
        destroyGTFtree(t);
        sam_close(fp);
    }

    findOverlapsBAM2(t, fp, hdr, matchType, strandType, minMapq);

    destroyGTFtree(t);
    bam_hdr_destroy(hdr);
    sam_close(fp);
    return 0;
}

#include "Variant.h"
#include <getopt.h>


using namespace std;
using namespace vcf;

void printSummary(char** argv) {
    cerr << "usage: " << argv[0] << " [vcf file] [vcf file] ..." << endl
         << endl
         << "Combines VCF files positionally, combining samples when sites and alleles are identical." << endl
         << "Any number of VCF files may be combined.  The INFO field and other columns are taken from" << endl
         << "one of the files which are combined when records in multiple files match.  Alleles must" << endl
         << "have identical ordering to be combined into one record.  If they do not, multiple records" << endl
         << "will be emitted." << endl
         << endl
         << "options:" << endl
         << "    -h --help           This text." << endl
         << "    -r --region REGION  A region specifier of the form chrN:x-y to bound the merge" << endl;
    exit(1);
}

int main(int argc, char** argv) {

    if (argc < 2) {
        printSummary(argv);
    }

    string region;

    int c;
    while (true) {
        static struct option long_options[] =
        {
            /* These options set a flag. */
            //{"verbose", no_argument,       &verbose_flag, 1},
            {"help", no_argument, 0, 'h'},
            {"region", required_argument, 0, 'r'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "hr:",
                         long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {

        case 'h':
            printSummary(argv);
            break;

        case 'r':
            region = optarg;
            break;
            
        case '?':
            printSummary(argv);
            exit(1);
            break;
                
        default:
            abort ();
        }
    }

    vector<string> sampleNames;
    string randomHeader;
    VariantCallFile* vcf;

    // structure to track ordered variants
    //ChromNameCompare chromCompare;

    typedef 
        map<vector<string>, // alts
            map<VariantCallFile*, Variant*> >
        Position;

    typedef
        map<long int, Position>
        ChromVariants;

    typedef
        map<string, // chrom
            ChromVariants,
            ChromNameCompare>
        VariantsByChromPosAltFile;

    VariantsByChromPosAltFile  variantsByChromPosAltFile;

    VariantCallFile* firstVCF = NULL;
    for (int i = optind; i != argc; ++i) {
        string inputFilename = argv[i];
        vcf = new VariantCallFile;
        vcf->open(inputFilename);
        if (!region.empty()) {
            if (!vcf->setRegion(region)) {
                cerr << "could not set region on " << inputFilename << endl;
                delete vcf;
                continue;
            }
        }
        if (vcf->is_open()) {
            Variant* var = new Variant(*vcf);
            if (vcf->getNextVariant(*var)) {
                variantsByChromPosAltFile[var->sequenceName][var->position][var->alt][vcf] = var;
                sampleNames.insert(sampleNames.end(), vcf->sampleNames.begin(), vcf->sampleNames.end());
                // the first file is tracked for header generation
            }
            if (firstVCF == NULL) firstVCF = vcf;
        }
    }

    // get sorted, unique samples in all files
    sort(sampleNames.begin(), sampleNames.end());
    sampleNames.erase(unique(sampleNames.begin(), sampleNames.end()), sampleNames.end());

    // now that we've accumulated the sample information we can generate the combined header
    VariantCallFile outputCallFile;
    string header = firstVCF->headerWithSampleNames(sampleNames);
    outputCallFile.openForOutput(header);

    cout << outputCallFile.header << endl;

    while (!variantsByChromPosAltFile.empty()) {
        // get lowest variant(s)
        // if they have identical alts and position, combine
        // otherwise just output, but with the same sample names

        ChromVariants& chrom = variantsByChromPosAltFile.begin()->second;
        if (chrom.empty()) {
            variantsByChromPosAltFile.erase(variantsByChromPosAltFile.begin());
            continue;
        }
        
        Position& pos = chrom.begin()->second;
        Position::iterator s = pos.begin();
        for ( ; s != pos.end(); ++s) {
            Variant variant(outputCallFile);
            map<VariantCallFile*, Variant*>& vars = s->second;
            map<VariantCallFile*, Variant*>::iterator v = vars.begin();
            for ( ; v != vars.end(); ++v) {
                VariantCallFile* vcf = v->first;
                Variant* var = v->second;
                //if (variant.info.empty()) {
                if (v == vars.begin()) { // set these using the first matching variant
                    variant.sequenceName = var->sequenceName;
                    variant.position = var->position;
                    variant.id = var->id;
                    variant.ref = var->ref;
                    variant.alt = var->alt;
                    variant.filter = var->filter;
                    variant.quality = var->quality;
                    variant.info = var->info;
                    variant.format = var->format;
                }
                // add samples to output variant
                for (Samples::iterator sample = var->samples.begin(); sample != var->samples.end(); ++sample) {
                    variant.samples[sample->first] = sample->second;
                }
                if (vcf->getNextVariant(*var)) {
                    variantsByChromPosAltFile[var->sequenceName][var->position][var->alt][vcf] = var;
                }
            }
            // what was this chck for?
            //if (!variant.info.empty())
            cout << variant << endl;
        }
        // XXXXXXX this will cause problems if the files have more than one record per position!!!
        chrom.erase(chrom.begin());
    }

    return 0;

}


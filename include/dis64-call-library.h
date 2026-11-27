/* Existing scalar Imports/Exports signatures; no classic hash or new wire ID. */
#ifndef DIS64_CALL_LIBRARY_H
#define DIS64_CALL_LIBRARY_H
#include "dis64-call-chain.h"

typedef struct Dis64CallLibraryLinks Dis64CallLibraryLinks;
struct Dis64CallLibraryLinks {
 unsigned int count;
 unsigned int entry[Dis64LocalFunctions];
 unsigned int frame[Dis64LocalFunctions];
};

/* Structural matching only. The runtime must additionally prove the COMPLETE
 * immutable library before using these coordinates to construct a Modlink.
 * Request bytes/pool and image remain accessible, immutable and disjoint from out. */
static int
dis64calllibrarymatch(const Dis64CallImage *image,const unsigned char *requests,unsigned int requestbytes,
 const unsigned char *strings,unsigned int stringbytes,Dis64CallLibraryLinks *output)
{
 Dis64CallLibraryLinks links={0};Dis64Section section,exports={0},poolsection={0};
 Dis64Bindings imported,exported;Dis64Binding in,out;DisStringPoolView pool,names;
 DisRuneView module,dependency,symbol,candidate;unsigned int i,j;int order,equal;
 if(!image || !output || image->module.profile!=0 || requestbytes>Dis64LocalBytes || stringbytes>Dis64LocalBytes ||
    !disstringpoolcheck(strings,stringbytes,65536,&pool) ||
    !dis64bindingscheck(requests,requestbytes,0,&pool,0,0,Dis64LocalFunctions,Dis64LocalParameters,&imported))return 0;
 for(i=0;i<image->envelope.sections;i++){
  if(!dis64section(&image->envelope,i,&section))return 0;
  if(section.kind==Dis64Exports)exports=section;
  if(section.kind==Dis64Strings)poolsection=section;
 }
 if(!disstringpoolcheck(image->envelope.data+poolsection.offset,poolsection.bytes,65536,&names) ||
    !disstringpoolget(&names,image->module.name,&module) ||
    !dis64bindingscheck(image->envelope.data+exports.offset,exports.bytes,1,&names,&image->types,image->instructions,
      Dis64LocalFunctions,Dis64LocalParameters,&exported))return 0;
 for(i=0;i<imported.count;i++){
  if(!dis64bindingget(&imported,i,&in) || !disstringpoolget(&pool,in.module,&dependency) ||
     !disrunecompare(&module,&dependency,&order) || order || !disstringpoolget(&pool,in.symbol,&symbol))return 0;
  for(j=0;j<exported.count;j++){
   if(!dis64bindingget(&exported,j,&out) || !disstringpoolget(&names,out.symbol,&candidate) ||
      !disrunecompare(&symbol,&candidate,&order))return 0;
   if(order)continue;
   if(!dis64signatureequal(&in.signature,&out.signature,&equal) || !equal)return 0;
   links.entry[i]=out.entry;links.frame[i]=out.frame;break;
  }
  if(j==exported.count)return 0;
 }
 links.count=imported.count;*output=links;return 1;
}
#endif

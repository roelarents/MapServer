#include "maptemplate.h"
#include "maphash.h"
#include "cgiutil.h"

/*
** Is a particular layer or group on, that is was it requested explicitly by the user.
*/
int isOn(mapservObj *msObj, char *name, char *group)
{
  int i;

  for(i=0;i<msObj->NumLayers;i++) {
    if(name && strcmp(msObj->Layers[i], name) == 0)  return(MS_TRUE);
    if(group && strcmp(msObj->Layers[i], group) == 0) return(MS_TRUE);
  }

  return(MS_FALSE);
}

/*!
 * This function set the map->layerorder
 * index order by the metadata collumn name
*/
int sortLayerByMetadata(mapObj *map, char* pszMetadata)
{
   int nLegendOrder1;
   int nLegendOrder2;
   char *pszLegendOrder1;
   char *pszLegendOrder2;
   int i, j;
   int tmp;

   if (!map) {
     msSetError(MS_WEBERR, "Invalid pointer.", "sortLayerByMetadata()");
     return MS_FAILURE;
   }
   
   if (map->layerorder)
     free(map->layerorder);

   map->layerorder = (int*)malloc(map->numlayers * sizeof(int));

   /*
    * Initiate to default order
    */
   for (i=0; i<map->numlayers; i++)
      map->layerorder[i] = i;
     
   if (!pszMetadata)
     return MS_SUCCESS;
   
   /* 
    * Bubble sort algo (not very efficient)
    * should implement a kind of quick sort
    * alog instead
   */
   for (i=0; i<map->numlayers-1; i++) {
      for (j=0; j<map->numlayers-1-i; j++) {
         pszLegendOrder1 = msLookupHashTable(map->layers[map->layerorder[j+1]].metadata, pszMetadata);
         pszLegendOrder2 = msLookupHashTable(map->layers[map->layerorder[j]].metadata, pszMetadata);
     
         if (!pszLegendOrder1 || !pszLegendOrder2)
           continue;
         
         nLegendOrder1 = atoi(pszLegendOrder1);
         nLegendOrder2 = atoi(pszLegendOrder2);      
         
         if (nLegendOrder1 < nLegendOrder2) {  /* compare the two neighbors */
            tmp = map->layerorder[j];         /* swap a[j] and a[j+1]      */
            map->layerorder[j] = map->layerorder[j+1];
            map->layerorder[j+1] = tmp;
         }
      }
   }
   
   return MS_SUCCESS;
}

/*!
 * This function return a pointer
 * at the begining of the first occurence
 * of pszTag in pszInstr.
 * 
 * Tag can be [TAG] or [TAG something]
*/
char* findTag(char* pszInstr, char* pszTag)
{
   char *pszTag1, *pszTag2, *pszStart;

   if (!pszInstr || !pszTag) {
     msSetError(MS_WEBERR, "Invalid pointer.", "findTag()");
     return NULL;
   }

   pszTag1 = (char*)malloc(strlen(pszTag) + 3);
   pszTag2 = (char*)malloc(strlen(pszTag) + 3);

   strcpy(pszTag1, "[");   
   strcat(pszTag1, pszTag);
   strcat(pszTag1, " ");
   
   strcpy(pszTag2, "[");      
   strcat(pszTag2, pszTag);
   strcat(pszTag2, "]");
   
   pszStart = strstr(pszInstr, pszTag1);
   if (pszStart == NULL)
      pszStart = strstr(pszInstr, pszTag2);

   free(pszTag1);
   free(pszTag2);
   
   return pszStart;
}

/*!
 * return a hashtableobj from instr of all
 * arguments. hashtable must be freed by caller.
 */
int getTagArgs(char* pszTag, char* pszInstr, char** pszNextInstr, hashTableObj *oHashTable)
{
   char *pszStart, *pszEnd, *pszArgs;
   int nLength;
   char **papszArgs, **papszVarVal;
   int nArgs, nDummy;
   int i;
   
   if (!pszTag || !pszInstr) {
     msSetError(MS_WEBERR, "Invalid pointer.", "getTagArgs()");
     return MS_FAILURE;
   }
   
   // set position to the begining of tag
   pszStart = findTag(pszInstr, pszTag);
   
   if (pszStart) {
      // find ending position
      pszEnd = strchr(pszStart, ']');
   
      if (pszEnd) {
         // skit the tag name
         pszStart = pszStart + strlen(pszTag) + 1;

         if (pszNextInstr)
           *pszNextInstr = pszEnd + 1;
         
         // get lenght of all args
         nLength = pszEnd - pszStart;
   
         if (nLength > 0) { // is there arguments ?
            pszArgs = (char*)malloc(nLength + 1);
            strncpy(pszArgs, pszStart, nLength);
            pszArgs[nLength] = '\0';
            
            if (!*oHashTable)
              *oHashTable = msCreateHashTable();
            
            // put all arguments seperate by space in a hash table
            papszArgs = split(pszArgs, ' ', &nArgs);

            // check all argument if they have values
            for (i=0; i<nArgs; i++) {
               if (strchr(papszArgs[i], '='))
               {
                  papszVarVal = split(papszArgs[i], '=', &nDummy);
               
                  msInsertHashTable(*oHashTable, papszVarVal[0], papszVarVal[1]);
                  free(papszVarVal[0]);
                  free(papszVarVal[1]);
                  free(papszVarVal);                  
               }
               else // no value specified. set it to 1
                  msInsertHashTable(*oHashTable, papszArgs[i], "1");
               
               free(papszArgs[i]);
            }
            free(papszArgs);
         }
      }
   }  

   return MS_SUCCESS;
}   

/*!
 * return a substring from instr beetween [tag] and [/tag]
 * char* returned must be freed by caller.
 * pszNextInstr will be a pointer at the end of the 
 * first occurence founded.
 */
int getInlineTag(char* pszTag, char* pszInstr, char** pszNextInstr, char **pszResult)
{
   char *pszStart, *pszEnd,  *pszEndTag;
   int nLength;

   *pszResult = NULL;

   if (!pszInstr || !pszTag) {
     msSetError(MS_WEBERR, "Invalid pointer.", "getInlineTag()");
     return MS_FAILURE;
   }
   
   // find start tag
   pszStart = findTag(pszInstr, pszTag);
   
   if (pszStart) {
      // find end of start tag
      pszStart = strchr(pszStart, ']');
   
      if (pszStart) {
         pszStart++;
   
         pszEndTag = (char*)malloc(strlen(pszTag) + 3);
         strcpy(pszEndTag, "[/");
         strcat(pszEndTag, pszTag);

         // find start of end tag
         pszEnd = strstr(pszStart, pszEndTag);
         
         if (pszEnd) {
            nLength = pszEnd - pszStart;
            
            if (nLength > 0) {
               *pszResult = (char*)malloc(nLength + 1);

               // copy string beetween start and end tag
               strncpy(*pszResult, pszStart, nLength);

               (*pszResult)[nLength] = '\0';
               
               if (pszNextInstr)
                 *pszNextInstr = pszStart + nLength + strlen(pszTag) + 2;
            }
         }
      }
   }
   
   return MS_SUCCESS;
}

/*!
 * this function process all if tag in pszInstr.
 * this function return a modified pszInstr.
 * ht mus contain all variables needed by the function
 * to interpret if expression.
*/
int processIf(char* pszInstr, hashTableObj ht)
{
   char *pszNextInstr = pszInstr;
   char *pszStart, *pszEnd;
   char *pszName, *pszValue, *pszOperator, *pszThen=NULL;
   char *pszIfTag;
   int nLength;
   
   hashTableObj ifArgs=NULL;

   if (!ht || !pszInstr) {
     msSetError(MS_WEBERR, "Invalid pointer.", "processIf()");
     return MS_FAILURE;
   }
   
   do
   {
      // find the if start tag
      pszStart = findTag(pszNextInstr, "if");
      
      if (pszStart)
      {
         // get the then string (if expression is true)
         if (getInlineTag("if", pszStart, NULL, &pszThen) != MS_SUCCESS)
           return MS_FAILURE;
         
         // retrieve if tag args
         if (getTagArgs("if", pszStart, &pszNextInstr, &ifArgs) != MS_SUCCESS)
           return MS_FAILURE;
         
         pszName = msLookupHashTable(ifArgs, "name");
         pszValue = msLookupHashTable(ifArgs, "value");
         pszOperator = msLookupHashTable(ifArgs, "operator"); // ignored for the momment
         
         if (pszName && pszValue && msLookupHashTable(ht, pszName)) {
            // set position at end of if start tag
            pszEnd = strchr(pszStart, ']');
            pszEnd++;

            // build the complete if tag ([if all_args]then string[/if])
            // to replace if by then string if expression is true
            // or by a white space if not.
            nLength = pszEnd - pszStart;
            pszIfTag = (char*)malloc(nLength + strlen(pszThen) + 6);
            strncpy(pszIfTag, pszStart, nLength);
            pszIfTag[nLength] = '\0';
            
            strcat(pszIfTag, pszThen);
            strcat(pszIfTag, "[/if]");


            if (strcasecmp(pszValue, msLookupHashTable(ht, pszName)) == 0)
              pszInstr = gsub(pszInstr, pszIfTag, pszThen);
            else
              pszInstr = gsub(pszInstr, pszIfTag, "");

            free(pszIfTag);
         }
         else {
            if (pszThen)
              free(pszThen);
            if (ifArgs)
              msFreeHashTable(ifArgs);
            
            msSetError(MS_WEBERR, "Malformed if tag.", "processIf()");
            return MS_FAILURE;
         }    

         free (pszThen);
         msFreeHashTable(ifArgs);
         
         pszStart = pszNextInstr;
      }
      
   }
   while (pszStart);
   
   return MS_SUCCESS;
}

/*!
 * this function process all metadata
 * in pszInstr. ht mus contain all corresponding
 * metadata value.
 * 
 * this function return a modified pszInstr
*/
int processMetadata(char* pszInstr, hashTableObj ht)
{
   char *pszNextInstr = pszInstr;
   char *pszEnd, *pszStart;
   char *pszMetadataTag;
   char *pszHashName;
   char *pszHashValue;
   int nLength;

   hashTableObj metadataArgs = NULL;
   
   if (!ht || !pszInstr) {
     msSetError(MS_WEBERR, "Invalid pointer.", "processMetadata()");
     return MS_FAILURE;
   }
   
   // for all metadata in pszInstr
   do {
      // set position to the begining of metadata tag
      pszStart = findTag(pszNextInstr, "metadata");
      
      if (pszStart)
      {
         // get metadata args
         if (getTagArgs("metadata", pszStart, &pszNextInstr, &metadataArgs) != MS_SUCCESS)
           return MS_FAILURE;

         pszHashName = msLookupHashTable(metadataArgs, "name");
         pszHashValue = msLookupHashTable(ht, pszHashName);
         
         if (pszHashName && pszHashValue) {
            // set position to the end of metadata start tag
            pszEnd = strchr(pszStart, ']');
            pszEnd++;

            // build the complete metadata tag ([metadata all_args])
            // to replace it by the corresponding value from ht
            nLength = pszEnd - pszStart;
            pszMetadataTag = (char*)malloc(nLength + 1);
            strncpy(pszMetadataTag, pszStart, nLength);
            pszMetadataTag[nLength] = '\0';

            pszStart = gsub(pszInstr, pszMetadataTag, pszHashValue);

            free(pszMetadataTag);
         }
         else {
            if (metadataArgs)
              msFreeHashTable(metadataArgs);              
            msSetError(MS_WEBERR, "Malformed metadata tag.", "processMetadataL()");
            return MS_FAILURE;
         }

         msFreeHashTable(metadataArgs);

         pszStart = pszNextInstr;
      }
      
   } while(pszStart);
   
   return MS_SUCCESS;
}

/*!
 * this function process all icon tag
 * from pszInstr.
 * 
 * This func return a modified pszInstr.
*/
int processIcon(mapObj *map, int nIdxLayer, int nIdxClass, char** pszInstr)
{
   int nWidth, nHeight, nLen;
   char *pszImgFname, *pszFullImgFname, *pszImgTag;
   gdImagePtr img;
   hashTableObj myHashTable=NULL;
   
   if (!map || 
       nIdxLayer > map->numlayers || 
       nIdxLayer < 0 || 
       nIdxClass > map->layers[nIdxLayer].numclasses || 
       nIdxClass < 0) {
     msSetError(MS_WEBERR, "Invalid pointer.", "processIcon()");
     return MS_FAILURE;
   }

   if (getTagArgs("leg_icon", *pszInstr, NULL, &myHashTable) != MS_SUCCESS)
     return MS_FAILURE;

   // if no specified width or height, set them to map default
   if (!msLookupHashTable(myHashTable, "width") || !msLookupHashTable(myHashTable, "height")) {
     nWidth = map->legend.keysizex;
     nHeight= map->legend.keysizey;
   }
   else {
      nWidth  = atoi(msLookupHashTable(myHashTable, "width"));
      nHeight = atoi(msLookupHashTable(myHashTable, "height"));
   }
   
   // Create an image corresponding to the current class
   img = msCreateLegendIcon(map, &(map->layers[nIdxLayer]), &(map->layers[nIdxLayer].class[nIdxClass]), nWidth, nHeight);

   if(!img) {
     if (myHashTable)
        msFreeHashTable(myHashTable);
      
     msSetError(MS_GDERR, "Error while creating GD image.", "processIcon()");
     return MS_FAILURE;
   }

   // save it with a unique file name
   pszImgFname = msTmpFile("", MS_IMAGE_EXTENSION(map->imagetype));
   pszFullImgFname = (char*)malloc(strlen(map->web.imagepath) + strlen(pszImgFname) + 1);
   strcpy(pszFullImgFname, map->web.imagepath);
   strcat(pszFullImgFname, pszImgFname);

   if(msSaveImage(img, pszFullImgFname, map->imagetype, map->legend.transparent, map->legend.interlace, map->imagequality) == -1) {
     if (myHashTable)
        msFreeHashTable(myHashTable);
     if (pszImgFname)
        free(pszImgFname);
     if (pszFullImgFname)
        free(pszFullImgFname);
      
     msSetError(MS_IOERR, "Error while save GD image to disk.", "processIcon()");
     return MS_FAILURE;
   }
         
   free(pszFullImgFname);
         
   gdImageDestroy(img);

   // find the begining of tag
   pszImgTag = strstr(*pszInstr, "[leg_icon");
   if (pszImgTag) {
      nLen = (strchr(pszImgTag, ']') + 1) - pszImgTag;
   
      if (nLen > 0) {
         char *pszTag;

         // rebuid image tag ([leg_class_img all_args])
         // to replace it by the image url
         pszTag = (char*)malloc(nLen + 1);
         strncpy(pszTag, pszImgTag, nLen);
            
         pszTag[nLen] = '\0';

         pszFullImgFname = (char*)malloc(strlen(map->web.imageurl) + strlen(pszImgFname) + 1);
         strcpy(pszFullImgFname, map->web.imageurl);
         strcat(pszFullImgFname, pszImgFname);
            
         *pszInstr = gsub(*pszInstr, pszTag, pszFullImgFname);
            
         free(pszFullImgFname);
         free(pszImgFname);
      
         return MS_SUCCESS;
      }
   }

   free(pszImgFname);
   
   return MS_SUCCESS;
}

/*!
 * This function probably should be in
 * mapstring.c
 * 
 * it concatenate pszSrc to pszDest and reallocate
 * memory if necessary.
*/
char *strcatalloc(char* pszDest, char* pszSrc)
{
   int nLen;
   
   if (pszSrc == NULL)
      return pszDest;

   // if destination is null, allocate memory
   if (pszDest == NULL) {
      nLen = strlen(pszSrc);
      pszDest = (char*)malloc(nLen + 1);
      strcpy(pszDest, pszSrc);
      pszDest[nLen] = '\0';
   }
   else { // if dest is not null, reallocate memory
      char *pszTemp;
           
      nLen = strlen(pszDest) + strlen(pszSrc);
           
      pszTemp = (char*)realloc(pszDest, nLen + 1);
      if (pszTemp) {
         pszDest = pszTemp;
         strcat(pszDest, pszSrc);
         pszDest[nLen] = '\0';
      }
      else {
         msSetError(MS_WEBERR, "Error while reallocating memory.", "strcatalloc()");
         return NULL;
      }        
   }
   
   return pszDest;
}

/*!
 * Replace all tags from group template
 * with correct value.
 * 
 * this function return a buffer containing
 * the template with correct values.
 * 
 * buffer must be freed by caller.
*/
int generateGroupTemplate(char* pszGroupTemplate, mapObj *map, char* pszGroupName, char **legGroupHtmlCopy)
{
   char *pszClassImg;
   
   if (!pszGroupName || !pszGroupTemplate) {
     msSetError(MS_WEBERR, "Invalid pointer.", "generateGroupTemplate()");
     return MS_FAILURE;
   }
   
   /*
    * Work from a copy
    */
   *legGroupHtmlCopy = (char*)malloc(strlen(pszGroupTemplate) + 1);
   strcpy(*legGroupHtmlCopy, pszGroupTemplate);
         
   /*
    * Change group tags
    */
   *legGroupHtmlCopy = gsub(*legGroupHtmlCopy, "[leg_group_name]", pszGroupName);
      
   /*
    * Check if leg_icon tag exist
    * if so display the first layer first class icon
    */
   pszClassImg = strstr(*legGroupHtmlCopy, "[leg_icon");
   if (pszClassImg) {
      processIcon(map, 0, 0, legGroupHtmlCopy);
   }      
      
   return MS_SUCCESS;
}

/*!
 * Replace all tags from layer template
 * with correct value.
 * 
 * this function return a buffer containing
 * the template with correct values.
 * 
 * buffer must be freed by caller.
*/
int generateLayerTemplate(char *pszLayerTemplate, mapObj *map, int nIdxLayer, hashTableObj oLayerArgs, char **pszTemp)
{
   hashTableObj myHashTable;
   char pszStatus[2];
   int nOptFlag=0;
   char *pszOptFlag;
   char *pszClassImg;

   if (!pszLayerTemplate || 
       !map || 
       nIdxLayer > map->numlayers ||
       nIdxLayer < 0 ||
       !oLayerArgs || 
       map->layers[nIdxLayer].numclasses == 0) {
     msSetError(MS_WEBERR, "Invalid pointer.", "generateLayerTemplate()");
     return MS_FAILURE;
   }

   pszOptFlag = msLookupHashTable(oLayerArgs, "opt_flag");
   if (pszOptFlag)
     nOptFlag = atoi(pszOptFlag);
      
   // dont display layer is off.
   // check this if Opt flag is not set
   if((nOptFlag & 2) == 0 && map->layers[nIdxLayer].status == MS_OFF)
     return MS_SUCCESS;
      
   // dont display layer is query.
   // check this if Opt flag is not set      
   if ((nOptFlag & 4) == 0  && map->layers[nIdxLayer].type == MS_LAYER_QUERY)
     return MS_SUCCESS;

   // dont display layer is annotation.
   // check this if Opt flag is not set      
   if ((nOptFlag & 8) == 0 && map->layers[nIdxLayer].type == MS_LAYER_ANNOTATION)
     return MS_SUCCESS;      

   // dont display layer if out of scale.
   // check this if Opt flag is not set            
   if ((nOptFlag & 1) == 0) {
      if(map->scale > 0) {
         if((map->layers[nIdxLayer].maxscale > 0) && (map->scale > map->layers[nIdxLayer].maxscale))
           return MS_SUCCESS;
         if((map->layers[nIdxLayer].minscale > 0) && (map->scale <= map->layers[nIdxLayer].minscale))
           return MS_SUCCESS;
      }
   }

   /*
    * Work from a copy
    */
   *pszTemp = (char*)malloc(strlen(pszLayerTemplate) + 1);
   strcpy(*pszTemp, pszLayerTemplate);

   /*
    * Change layer tags
    */
   *pszTemp = gsub(*pszTemp, "[leg_layer_name]", map->layers[nIdxLayer].name);
   
   /*
    * Check if leg_icon tag exist
    * if so display the first class icon
    */
   pszClassImg = strstr(*pszTemp, "[leg_icon");
   if (pszClassImg) {
      processIcon(map, nIdxLayer, 0, pszTemp);
   }      

   processMetadata(*pszTemp, map->layers[nIdxLayer].metadata);
      
   /*
    * Create a hash table that contain info
    * on current layer
    */
   myHashTable = msCreateHashTable();
   
   /*
    * for now, only status is required by template
    */
   sprintf(pszStatus, "%d", map->layers[nIdxLayer].status);
   msInsertHashTable(myHashTable, "layer_status", pszStatus);

   processIf(*pszTemp, myHashTable);
      
   msFreeHashTable(myHashTable);
         
   return MS_SUCCESS;
}

/*!
 * Replace all tags from class template
 * with correct value.
 * 
 * this function return a buffer containing
 * the template with correct values.
 * 
 * buffer must be freed by caller.
*/
int generateClassTemplate(char* pszClassTemplate, mapObj *map, int nIdxLayer, int nIdxClass, hashTableObj oClassArgs, char **legClassHtmlCopy)
{
   char *pszClassImg;
   int nOptFlag=0;
   char *pszOptFlag;

   *legClassHtmlCopy = NULL;
   
   if (!pszClassTemplate || 
       !map || 
       nIdxLayer > map->numlayers ||
       nIdxLayer < 0 ||
       nIdxClass > map->layers[nIdxLayer].numclasses ||
       nIdxClass < 0) {
        
     msSetError(MS_WEBERR, "Invalid pointer.", "generateClassTemplate()");
     return MS_FAILURE;
   }
   
   pszOptFlag = msLookupHashTable(oClassArgs, "Opt_flag");
   if (pszOptFlag)
     nOptFlag = atoi(pszOptFlag);
      
   // dont display class if layer is off.
   // check this if Opt flag is not set
   if((nOptFlag & 2) == 0 && map->layers[nIdxLayer].status == MS_OFF)
     return MS_SUCCESS;

   // dont display class if layer is query.
   // check this if Opt flag is not set      
   if ((nOptFlag & 4) == 0 && map->layers[nIdxLayer].type == MS_LAYER_QUERY)
     return MS_SUCCESS;
      
   // dont display class if layer is annotation.
   // check this if Opt flag is not set      
   if ((nOptFlag & 8) == 0 && map->layers[nIdxLayer].type == MS_LAYER_ANNOTATION)
     return MS_SUCCESS;
      
   // dont display layer if out of scale.
   // check this if Opt flag is not set
   if ((nOptFlag & 1) == 0) {
      if(map->scale > 0) {
         if((map->layers[nIdxLayer].maxscale > 0) && (map->scale > map->layers[nIdxLayer].maxscale))
           return MS_SUCCESS;
         if((map->layers[nIdxLayer].minscale > 0) && (map->scale <= map->layers[nIdxLayer].minscale))
           return MS_SUCCESS;
      }
   }
      
   /*
    * Work from a copy
    */
   *legClassHtmlCopy = (char*)malloc(strlen(pszClassTemplate) + 1);
   strcpy(*legClassHtmlCopy, pszClassTemplate);
         
   /*
    * Change class tags
    */
   *legClassHtmlCopy = gsub(*legClassHtmlCopy, "[leg_class_name]", map->layers[nIdxLayer].class[nIdxClass].name);
   *legClassHtmlCopy = gsub(*legClassHtmlCopy, "[leg_class_title]", map->layers[nIdxLayer].class[nIdxClass].title);
      
   /*
    * Check if leg_icon tag exist
    */
   pszClassImg = strstr(*legClassHtmlCopy, "[leg_icon");
   if (pszClassImg) {
      processIcon(map, nIdxLayer, nIdxClass, legClassHtmlCopy);
   }

   return MS_SUCCESS;
}

char *generateLegendTemplate(mapObj *map)
{
   FILE *stream;
   char *file = NULL;
   int length;
   char *pszResult = NULL;
   char *legGroupHtml = NULL;
   char *legLayerHtml = NULL;
   char *legClassHtml = NULL;
   char *legLayerHtmlCopy = NULL;
   char *legClassHtmlCopy = NULL;
   char *legGroupHtmlCopy = NULL;
   
   char *pszOrderMetadata = NULL;
   
   int i,j,k;
   char **papszGroups = NULL;
   int nGroupNames = 0;

   int nLegendOrder = 0;
   char *pszOrderValue;
     
   hashTableObj groupArgs = NULL;
   hashTableObj layerArgs = NULL;
   hashTableObj classArgs = NULL;     

   regex_t re; /* compiled regular expression to be matched */ 

   if(regcomp(&re, MS_TEMPLATE_EXPR, REG_EXTENDED|REG_NOSUB) != 0) {
      msSetError(MS_IOERR, "Error regcomp.", "generateLegendTemplate()");      
      return NULL;
   }

   if(regexec(&re, map->legend.template, 0, NULL, 0) != 0) { /* no match */
      msSetError(MS_IOERR, "Invalid template file name.", "generateLegendTemplate()");      
     regfree(&re);
     return NULL;
   }
   regfree(&re);

   // open template
   if((stream = fopen(map->legend.template, "r")) == NULL) {
      msSetError(MS_IOERR, "Error while opening template file.", "generateLegendTemplate()");
      return NULL;
   } 

   fseek(stream, 0, SEEK_END);
   length = ftell(stream);
   rewind(stream);
   
   file = (char*)malloc(length + 1);

   if (!file) {
     msSetError(MS_IOERR, "Error while allocating memory for template file.", "generateLegendTemplate()");
     return NULL;
   }
   
   /*
    * Read all the template file
    */
   fread(file, 1, length, stream);
   file[length] = '\0';

   /*
    * Seperate groups, layers and class
    */
   if (getInlineTag("leg_group_html", file, NULL, &legGroupHtml) != MS_SUCCESS)
     return NULL;
   
   if (getInlineTag("leg_layer_html", file, NULL, &legLayerHtml) != MS_SUCCESS)
     return NULL;
   
   if (getInlineTag("leg_class_html", file, NULL, &legClassHtml) != MS_SUCCESS)
     return NULL;

   /*
    * Retrieve arguments of all three parts
    */
   if (legGroupHtml) 
     if (getTagArgs("leg_group_html", file, NULL, &groupArgs) != MS_SUCCESS)
       return NULL;
   
   if (legLayerHtml) 
     if (getTagArgs("leg_layer_html", file, NULL, &layerArgs) != MS_SUCCESS)
       return NULL;
   
   if (legClassHtml) 
     if (getTagArgs("leg_class_html", file, NULL, &classArgs) != MS_SUCCESS)
       return NULL;

      
   /********************************************************************/

   /*
    * order layers if order_metadata args is set
    * If not, keep default order
    */
   pszOrderMetadata = msLookupHashTable(layerArgs, "order_metadata");
      
   if (sortLayerByMetadata(map, pszOrderMetadata) != MS_SUCCESS)
     goto error;
      
   if (legGroupHtml) {
      // retrieve group names
      papszGroups = msGetAllGroupNames(map, &nGroupNames);

      for (i=0; i<nGroupNames; i++) {
         // process group tags
         if (generateGroupTemplate(legGroupHtml, map, papszGroups[i], &legGroupHtmlCopy) != MS_SUCCESS)
         {
            if (pszResult)
              free(pszResult);
            pszResult=NULL;
            goto error;
         }
            
         // concatenate it to final result
         pszResult = strcatalloc(pszResult, legGroupHtmlCopy);
            
         if (!pszResult)
         {
            if (pszResult)
              free(pszResult);
            pszResult=NULL;
            goto error;
         }
            
         if (legGroupHtmlCopy)
         {
           free(legGroupHtmlCopy);
           legGroupHtmlCopy = NULL;
         }
         

         // for all layers in group
         if (legLayerHtml) {
           for (j=0; j<map->numlayers; j++) {
              /*
               * if order_metadata is set and the order
               * value is less than 0, dont display it
               */
              pszOrderMetadata = msLookupHashTable(layerArgs, "order_metadata");
              if (pszOrderMetadata) {
                 pszOrderValue = msLookupHashTable(map->layers[map->layerorder[j]].metadata, pszOrderMetadata);
                 if (pszOrderValue)
                   nLegendOrder = atoi(pszOrderValue);
              }

              if (nLegendOrder >= 0 && map->layers[map->layerorder[j]].group && strcmp(map->layers[map->layerorder[j]].group, papszGroups[i]) == 0) {
                 // process all layer tags
                 if (generateLayerTemplate(legLayerHtml, map, map->layerorder[j], layerArgs, &legLayerHtmlCopy) != MS_SUCCESS)
                 {
                    if (pszResult)
                      free(pszResult);
                    pszResult=NULL;
                    goto error;
                 }
              
                  
                 // concatenate to final result
                 pszResult = strcatalloc(pszResult, legLayerHtmlCopy);

                 if (!pszResult)
                 {
                    if (pszResult)
                      free(pszResult);
                    pszResult=NULL;
                    goto error;
                 }
            
                  
                 if (legLayerHtmlCopy)
                   free(legLayerHtmlCopy);
            
                 // for all classes in layer
                 if (legClassHtml) {
                    for (k=0; k<map->layers[map->layerorder[j]].numclasses; k++) {
                       // process all class tags
                       if (!map->layers[map->layerorder[j]].class[k].name)
                         continue;

                       if (generateClassTemplate(legClassHtml, map, map->layerorder[j], k, classArgs, &legClassHtmlCopy) != MS_SUCCESS)
                       {
                          if (pszResult)
                            free(pszResult);
                          pszResult=NULL;
                          goto error;
                       }
                 
               
                       // concatenate to final result
                       pszResult = strcatalloc(pszResult, legClassHtmlCopy);
                     
                       if (!pszResult)
                       {
                          if (pszResult)
                            free(pszResult);
                          pszResult=NULL;
                          goto error;
                       }
               

                       if (legClassHtmlCopy) {
                         free(legClassHtmlCopy);
                         legClassHtmlCopy = NULL;
                       }
                    }
                 }
              }
           }
         }
      }
   }
   else {
      // if no group template specified
      if (legLayerHtml) {
         for (j=0; j<map->numlayers; j++) {
            /*
             * if order_metadata is set and the order
             * value is less than 0, dont display it
             */
            pszOrderMetadata = msLookupHashTable(layerArgs, "order_metadata");
            if (pszOrderMetadata) {
               pszOrderValue = msLookupHashTable(map->layers[map->layerorder[j]].metadata, pszOrderMetadata);
               if (pszOrderValue) {
                  nLegendOrder = atoi(pszOrderValue);
                  if (nLegendOrder < 0)
                    continue;
               }
            }

            // process a layer tags
            if (generateLayerTemplate(legLayerHtml, map, map->layerorder[j], layerArgs, &legLayerHtmlCopy) != MS_SUCCESS)
            {
               if (pszResult)
                 free(pszResult);
               pszResult=NULL;
               goto error;
            }
              
            // concatenate to final result
            pszResult = strcatalloc(pszResult, legLayerHtmlCopy);
            
            if (!pszResult)
            {
               if (pszResult)
                 free(pszResult);
               pszResult=NULL;
               goto error;
            }
  

            if (legLayerHtmlCopy) {
               free(legLayerHtmlCopy);
               legLayerHtmlCopy = NULL;
            }
            
            // for all classes in layer
            if (legClassHtml) {
               for (k=0; k<map->layers[map->layerorder[j]].numclasses; k++) {
                  // process all class tags
                  if (!map->layers[map->layerorder[j]].class[k].name)
                    continue;

                  if (generateClassTemplate(legClassHtml, map, map->layerorder[j], k, classArgs, &legClassHtmlCopy) != MS_SUCCESS)
                  {
                     if (pszResult)
                       free(pszResult);
                     pszResult=NULL;
                     goto error;
                  }
          
               
                  // concatenate to final result
                  pszResult = strcatalloc(pszResult, legClassHtmlCopy);
                  
                  if (!pszResult)
                  {
                     if (pszResult)
                       free(pszResult);
                     pszResult=NULL;
                     goto error;
                  }
              
               
                  if (legClassHtmlCopy) {
                    free(legClassHtmlCopy);
                    legClassHtmlCopy = NULL;
                  }
               }
            }         
         }
      }
      else { // if no group and layer template specified
         if (legClassHtml) {
            for (j=0; j<map->numlayers; j++) {
               /*
                * if order_metadata is set and the order
                * value is less than 0, dont display it
                */
               pszOrderMetadata = msLookupHashTable(layerArgs, "order_metadata");
               if (pszOrderMetadata) {
                  pszOrderValue = msLookupHashTable(map->layers[map->layerorder[j]].metadata, pszOrderMetadata);
                  if (pszOrderValue) {
                     nLegendOrder = atoi(pszOrderValue);
                     if (nLegendOrder < 0)
                       continue;
                  }
               }
               
               for (k=0; k<map->layers[map->layerorder[j]].numclasses; k++) {
                  if (!map->layers[map->layerorder[j]].class[k].name)
                    continue;
                  
                  if (generateClassTemplate(legClassHtml, map, map->layerorder[j], k, classArgs, &legClassHtmlCopy) != MS_SUCCESS)
                  {
                     if (pszResult)
                       free(pszResult);
                     pszResult=NULL;
                     goto error;
                  }
      
               
                  pszResult = strcatalloc(pszResult, legClassHtmlCopy);
               
                  if (!pszResult)
                  {
                     if (pszResult)
                       free(pszResult);
                     pszResult=NULL;
                     goto error;
                  }

                  if (legClassHtmlCopy) {
                    free(legClassHtmlCopy);
                    legClassHtmlCopy = NULL;
                  }
               }
            }
         }
      }
   }
   
   /********************************************************************/
      
   error:
      
   if (papszGroups) {
      for (i=0; i<nGroupNames; i++)
        free(papszGroups[i]);

      free(papszGroups);
   }
   
   msFreeHashTable(groupArgs);
   msFreeHashTable(layerArgs);
   msFreeHashTable(classArgs);
   
   if (file)
     free(file);
     
   if (legGroupHtmlCopy){ free(legGroupHtmlCopy); }
   if (legLayerHtmlCopy){ free(legLayerHtmlCopy); }
   if (legClassHtmlCopy){ free(legClassHtmlCopy); }
      
   if (legGroupHtml){ free(legGroupHtml); }
   if (legLayerHtml){ free(legLayerHtml); }
   if (legClassHtml){ free(legClassHtml); }
   
   fclose(stream);
   
   return pszResult;
}

char *processLine(mapservObj* msObj, char* instr, int mode)
{
  int i, j;
  //char repstr[1024], substr[1024], *outstr; // repstr = replace string, substr = sub string
  char repstr[5120], substr[5120], *outstr;
  struct hashObj *tp=NULL;
  char *encodedstr;
   
#ifdef USE_PROJ
  rectObj llextent;
  pointObj llpoint;
#endif

  outstr = strdup(instr); // work from a copy
  
 outstr = gsub(outstr, "[version]",  msGetVersion());

  sprintf(repstr, "%s%s%s.%s", msObj->Map->web.imageurl, msObj->Map->name, msObj->Id, MS_IMAGE_EXTENSION(msObj->Map->imagetype));
  outstr = gsub(outstr, "[img]", repstr);
  sprintf(repstr, "%s%sref%s.%s", msObj->Map->web.imageurl, msObj->Map->name, msObj->Id, MS_IMAGE_EXTENSION(msObj->Map->imagetype));
  outstr = gsub(outstr, "[ref]", repstr);

   
  if (strstr(outstr, "[legend]")) {
     // if there's a template legend specified, use it
     if (msObj->Map->legend.template) {
        char *legendTemplate;

        legendTemplate = generateLegendTemplate(msObj->Map);
        if (legendTemplate) {
          outstr = gsub(outstr, "[legend]", legendTemplate);
     
           free(legendTemplate);
        }
        else // error already generat by (generateLegendTemplate())
          return NULL;
     }
     else { // if not display gif image with all legend icon
        sprintf(repstr, "%s%sleg%s.%s", msObj->Map->web.imageurl, msObj->Map->name, msObj->Id, MS_IMAGE_EXTENSION(msObj->Map->imagetype));
        outstr = gsub(outstr, "[legend]", repstr);
     }
  }
   
  sprintf(repstr, "%s%ssb%s.%s", msObj->Map->web.imageurl, msObj->Map->name, msObj->Id, MS_IMAGE_EXTENSION(msObj->Map->imagetype));
  outstr = gsub(outstr, "[scalebar]", repstr);

  if(msObj->SaveQuery) {
    sprintf(repstr, "%s%s%s%s", msObj->Map->web.imagepath, msObj->Map->name, msObj->Id, MS_QUERY_EXTENSION);
    outstr = gsub(outstr, "[queryfile]", repstr);
  }
  
  if(msObj->SaveMap) {
    sprintf(repstr, "%s%s%s.map", msObj->Map->web.imagepath, msObj->Map->name, msObj->Id);
    outstr = gsub(outstr, "[map]", repstr);
  }

  sprintf(repstr, "%s", getenv("HTTP_HOST")); 
  outstr = gsub(outstr, "[host]", repstr);
  sprintf(repstr, "%s", getenv("SERVER_PORT"));
  outstr = gsub(outstr, "[port]", repstr);
  
  sprintf(repstr, "%s", msObj->Id);
  outstr = gsub(outstr, "[id]", repstr);
  
  strcpy(repstr, ""); // Layer list for a "GET" request
  for(i=0;i<msObj->NumLayers;i++)    
    sprintf(repstr, "%s&layer=%s", repstr, msObj->Layers[i]);
  outstr = gsub(outstr, "[get_layers]", repstr);
  
  strcpy(repstr, ""); // Layer list for a "POST" request
  for(i=0;i<msObj->NumLayers;i++)
    sprintf(repstr, "%s%s ", repstr, msObj->Layers[i]);
  trimBlanks(repstr);
  outstr = gsub(outstr, "[layers]", repstr);

  encodedstr = encode_url(repstr);
  outstr = gsub(outstr, "[layers_esc]", encodedstr);
  free(encodedstr);

  for(i=0;i<msObj->Map->numlayers;i++) { // Set form widgets (i.e. checkboxes, radio and select lists), note that default layers don't show up here
    if(isOn(msObj, msObj->Map->layers[i].name, msObj->Map->layers[i].group) == MS_TRUE) {
      if(msObj->Map->layers[i].group) {
	sprintf(substr, "[%s_select]", msObj->Map->layers[i].group);
	outstr = gsub(outstr, substr, "selected");
	sprintf(substr, "[%s_check]", msObj->Map->layers[i].group);
	outstr = gsub(outstr, substr, "checked");
      }
      sprintf(substr, "[%s_select]", msObj->Map->layers[i].name);
      outstr = gsub(outstr, substr, "selected");
      sprintf(substr, "[%s_check]", msObj->Map->layers[i].name);
      outstr = gsub(outstr, substr, "checked");
    } else {
      if(msObj->Map->layers[i].group) {
	sprintf(substr, "[%s_select]", msObj->Map->layers[i].group);
	outstr = gsub(outstr, substr, "");
	sprintf(substr, "[%s_check]", msObj->Map->layers[i].group);
	outstr = gsub(outstr, substr, "");
      }
      sprintf(substr, "[%s_select]", msObj->Map->layers[i].name);
      outstr = gsub(outstr, substr, "");
      sprintf(substr, "[%s_check]", msObj->Map->layers[i].name);
      outstr = gsub(outstr, substr, "");
    }
  }

  for(i=-1;i<=1;i++) { /* make zoom direction persistant */
    if(msObj->ZoomDirection == i) {
      sprintf(substr, "[zoomdir_%d_select]", i);
      outstr = gsub(outstr, substr, "selected");
      sprintf(substr, "[zoomdir_%d_check]", i);
      outstr = gsub(outstr, substr, "checked");
    } else {
      sprintf(substr, "[zoomdir_%d_select]", i);
      outstr = gsub(outstr, substr, "");
      sprintf(substr, "[zoomdir_%d_check]", i);
      outstr = gsub(outstr, substr, "");
    }
  }
  
  for(i=MINZOOM;i<=MAXZOOM;i++) { /* make zoom persistant */
    if(msObj->Zoom == i) {
      sprintf(substr, "[zoom_%d_select]", i);
      outstr = gsub(outstr, substr, "selected");
      sprintf(substr, "[zoom_%d_check]", i);
      outstr = gsub(outstr, substr, "checked");
    } else {
      sprintf(substr, "[zoom_%d_select]", i);
      outstr = gsub(outstr, substr, "");
      sprintf(substr, "[zoom_%d_check]", i);
      outstr = gsub(outstr, substr, "");
    }
  }

  // allow web object metadata access in template
  if(msObj->Map->web.metadata && strstr(outstr, "web_")) {
    for(j=0; j<MS_HASHSIZE; j++) {
      if (msObj->Map->web.metadata[j] != NULL) {
	for(tp=msObj->Map->web.metadata[j]; tp!=NULL; tp=tp->next) {            
	  sprintf(substr, "[web_%s]", tp->key);
	  outstr = gsub(outstr, substr, tp->data);  
	  sprintf(substr, "[web_%s_esc]", tp->key);
       
      encodedstr = encode_url(tp->data);
	  outstr = gsub(outstr, substr, encodedstr);
      free(encodedstr);
	}
      }
    }
  }

  // allow layer metadata access in template
  for(i=0;i<msObj->Map->numlayers;i++) {
    if(msObj->Map->layers[i].metadata && strstr(outstr, msObj->Map->layers[i].name)) {
      for(j=0; j<MS_HASHSIZE; j++) {
	if (msObj->Map->layers[i].metadata[j] != NULL) {
	  for(tp=msObj->Map->layers[i].metadata[j]; tp!=NULL; tp=tp->next) {            
	    sprintf(substr, "[%s_%s]", msObj->Map->layers[i].name, tp->key);
	    if(msObj->Map->layers[i].status == MS_ON)
	      outstr = gsub(outstr, substr, tp->data);  
	    else
	      outstr = gsub(outstr, substr, "");
	    sprintf(substr, "[%s_%s_esc]", msObj->Map->layers[i].name, tp->key);
	    if(msObj->Map->layers[i].status == MS_ON) {
          encodedstr = encode_url(tp->data);
          outstr = gsub(outstr, substr, encodedstr);
          free(encodedstr);
        }
	    else
	      outstr = gsub(outstr, substr, "");
	  }
	}
      }
    }
  }


  sprintf(repstr, "%f", msObj->MapPnt.x);
  outstr = gsub(outstr, "[mapx]", repstr);
  sprintf(repstr, "%f", msObj->MapPnt.y);
  outstr = gsub(outstr, "[mapy]", repstr);
  
  sprintf(repstr, "%f", msObj->Map->extent.minx); // Individual mapextent elements for spatial query building 
  outstr = gsub(outstr, "[minx]", repstr);
  sprintf(repstr, "%f", msObj->Map->extent.maxx);
  outstr = gsub(outstr, "[maxx]", repstr);
  sprintf(repstr, "%f", msObj->Map->extent.miny);
  outstr = gsub(outstr, "[miny]", repstr);
  sprintf(repstr, "%f", msObj->Map->extent.maxy);
  outstr = gsub(outstr, "[maxy]", repstr);
  sprintf(repstr, "%f %f %f %f", msObj->Map->extent.minx, msObj->Map->extent.miny,  msObj->Map->extent.maxx, msObj->Map->extent.maxy);
  outstr = gsub(outstr, "[mapext]", repstr);
   
  encodedstr =  encode_url(repstr);
  outstr = gsub(outstr, "[mapext_esc]", encodedstr);
  free(encodedstr);
  
  sprintf(repstr, "%f", msObj->RawExt.minx); // Individual raw extent elements for spatial query building
  outstr = gsub(outstr, "[rawminx]", repstr);
  sprintf(repstr, "%f", msObj->RawExt.maxx);
  outstr = gsub(outstr, "[rawmaxx]", repstr);
  sprintf(repstr, "%f", msObj->RawExt.miny);
  outstr = gsub(outstr, "[rawminy]", repstr);
  sprintf(repstr, "%f", msObj->RawExt.maxy);
  outstr = gsub(outstr, "[rawmaxy]", repstr);
  sprintf(repstr, "%f %f %f %f", msObj->RawExt.minx, msObj->RawExt.miny,  msObj->RawExt.maxx, msObj->RawExt.maxy);
  outstr = gsub(outstr, "[rawext]", repstr);
  
  encodedstr = encode_url(repstr);
  outstr = gsub(outstr, "[rawext_esc]", encodedstr);
  free(encodedstr);
    
#ifdef USE_PROJ
  if((strstr(outstr, "lat]") || strstr(outstr, "lon]") || strstr(outstr, "lon_esc]"))
     && msObj->Map->projection.proj != NULL
     && !pj_is_latlong(msObj->Map->projection.proj) ) {
    llextent=msObj->Map->extent;
    llpoint=msObj->MapPnt;
    msProjectRect(&(msObj->Map->projection), &(msObj->Map->latlon), &llextent);
    msProjectPoint(&(msObj->Map->projection), &(msObj->Map->latlon), &llpoint);

    sprintf(repstr, "%f", llpoint.x);
    outstr = gsub(outstr, "[maplon]", repstr);
    sprintf(repstr, "%f", llpoint.y);
    outstr = gsub(outstr, "[maplat]", repstr);
    
    sprintf(repstr, "%f", llextent.minx); /* map extent as lat/lon */
    outstr = gsub(outstr, "[minlon]", repstr);
    sprintf(repstr, "%f", llextent.maxx);
    outstr = gsub(outstr, "[maxlon]", repstr);
    sprintf(repstr, "%f", llextent.miny);
    outstr = gsub(outstr, "[minlat]", repstr);
    sprintf(repstr, "%f", llextent.maxy);
    outstr = gsub(outstr, "[maxlat]", repstr);    
    sprintf(repstr, "%f %f %f %f", llextent.minx, llextent.miny,  llextent.maxx, llextent.maxy);
    outstr = gsub(outstr, "[mapext_latlon]", repstr);
     
    encodedstr = encode_url(repstr);
    outstr = gsub(outstr, "[mapext_latlon_esc]", encodedstr);
    free(encodedstr);
  }
#endif

  sprintf(repstr, "%d %d", msObj->Map->width, msObj->Map->height);
  outstr = gsub(outstr, "[mapsize]", repstr);
   
  encodedstr = encode_url(repstr);
  outstr = gsub(outstr, "[mapsize_esc]", encodedstr);
  free(encodedstr);

  sprintf(repstr, "%d", msObj->Map->width);
  outstr = gsub(outstr, "[mapwidth]", repstr);
  sprintf(repstr, "%d", msObj->Map->height);
  outstr = gsub(outstr, "[mapheight]", repstr);
  
  sprintf(repstr, "%f", msObj->Map->scale);
  outstr = gsub(outstr, "[scale]", repstr);
  
  sprintf(repstr, "%.1f %.1f", (msObj->Map->width-1)/2.0, (msObj->Map->height-1)/2.0);
  outstr = gsub(outstr, "[center]", repstr);
  sprintf(repstr, "%.1f", (msObj->Map->width-1)/2.0);
  outstr = gsub(outstr, "[center_x]", repstr);
  sprintf(repstr, "%.1f", (msObj->Map->height-1)/2.0);
  outstr = gsub(outstr, "[center_y]", repstr);      

  // These are really for situations with multiple result sets only, but often used in header/footer  
  sprintf(repstr, "%d", msObj->NR); // total number of results
  outstr = gsub(outstr, "[nr]", repstr);  
  sprintf(repstr, "%d", msObj->NL); // total number of layers with results
  outstr = gsub(outstr, "[nl]", repstr);

  if(msObj->ResultLayer) {
    sprintf(repstr, "%d", msObj->NLR); // total number of results within this layer
    outstr = gsub(outstr, "[nlr]", repstr);
    sprintf(repstr, "%d", msObj->RN); // sequential (eg. 1..n) result number within all layers
    outstr = gsub(outstr, "[rn]", repstr);
    sprintf(repstr, "%d", msObj->LRN); // sequential (eg. 1..n) result number within this layer
    outstr = gsub(outstr, "[lrn]", repstr);
    outstr = gsub(outstr, "[cl]", msObj->ResultLayer->name); // current layer name    
    // if(ResultLayer->description) outstr = gsub(outstr, "[cd]", ResultLayer->description); // current layer description
  }

  if(mode == QUERY) { // return shape and/or values	
    
    sprintf(repstr, "%f %f", (msObj->ResultShape.bounds.maxx + msObj->ResultShape.bounds.minx)/2, (msObj->ResultShape.bounds.maxy + msObj->ResultShape.bounds.miny)/2); 
    outstr = gsub(outstr, "[shpmid]", repstr);
    sprintf(repstr, "%f", (msObj->ResultShape.bounds.maxx + msObj->ResultShape.bounds.minx)/2);
    outstr = gsub(outstr, "[shpmidx]", repstr);
    sprintf(repstr, "%f", (msObj->ResultShape.bounds.maxy + msObj->ResultShape.bounds.miny)/2);
    outstr = gsub(outstr, "[shpmidy]", repstr);
    
    sprintf(repstr, "%f %f %f %f", msObj->ResultShape.bounds.minx, msObj->ResultShape.bounds.miny,  msObj->ResultShape.bounds.maxx, msObj->ResultShape.bounds.maxy);
    outstr = gsub(outstr, "[shpext]", repstr);
     
    encodedstr = encode_url(repstr);
    outstr = gsub(outstr, "[shpext_esc]", encodedstr);
    free(encodedstr);
     
    sprintf(repstr, "%f", msObj->ResultShape.bounds.minx);
    outstr = gsub(outstr, "[shpminx]", repstr);
    sprintf(repstr, "%f", msObj->ResultShape.bounds.miny);
    outstr = gsub(outstr, "[shpminy]", repstr);
    sprintf(repstr, "%f", msObj->ResultShape.bounds.maxx);
    outstr = gsub(outstr, "[shpmaxx]", repstr);
    sprintf(repstr, "%f", msObj->ResultShape.bounds.maxy);
    outstr = gsub(outstr, "[shpmaxy]", repstr);
    
    sprintf(repstr, "%ld", msObj->ResultShape.index);
    outstr = gsub(outstr, "[shpidx]", repstr);
    sprintf(repstr, "%d", msObj->ResultShape.tileindex);
    outstr = gsub(outstr, "[tileidx]", repstr);  

    for(i=0;i<msObj->ResultLayer->numitems;i++) {	 
      sprintf(substr, "[%s]", msObj->ResultLayer->items[i]);
      if(strstr(outstr, substr) != NULL)
	outstr = gsub(outstr, substr, msObj->ResultShape.values[i]);
      sprintf(substr, "[%s_esc]", msObj->ResultLayer->items[i]);
      if(strstr(outstr, substr) != NULL) {
        encodedstr = encode_url(msObj->ResultShape.values[i]);
        outstr = gsub(outstr, substr, encodedstr);
        free(encodedstr);
      }
    }
    
    // FIX: need to re-incorporate JOINS at some point
  }
  
  for(i=0;i<msObj->NumParams;i++) { 
    sprintf(substr, "[%s]", msObj->ParamNames[i]);
    outstr = gsub(outstr, substr, msObj->ParamValues[i]);
    sprintf(substr, "[%s_esc]", msObj->ParamNames[i]);
    
    encodedstr = encode_url(msObj->ParamValues[i]);
    outstr = gsub(outstr, substr, encodedstr);
    free(encodedstr);
  }

  return(outstr);
}

int msReturnPage(mapservObj* msObj, char* html, int mode)
{
  FILE *stream;
  char line[MS_BUFFER_LENGTH], *tmpline;

  regex_t re; /* compiled regular expression to be matched */ 

  if(regcomp(&re, MS_TEMPLATE_EXPR, REG_EXTENDED|REG_NOSUB) != 0) {
    msSetError(MS_REGEXERR, NULL, "msReturnPage()");
    return MS_FAILURE;
//    writeError();
  }

  if(regexec(&re, html, 0, NULL, 0) != 0) { /* no match */
    regfree(&re);
    msSetError(MS_WEBERR, "Malformed template name.", "msReturnPage()");
    return MS_FAILURE;
//    writeError();
  }
  regfree(&re);

  if((stream = fopen(html, "r")) == NULL) {
    msSetError(MS_IOERR, html, "msReturnPage()");
    return MS_FAILURE;
//    writeError();
  } 

  while(fgets(line, MS_BUFFER_LENGTH, stream) != NULL) { /* now on to the end of the file */

    if(strchr(line, '[') != NULL) {
      tmpline = processLine(msObj, line, mode);
      if (!tmpline)
         return MS_FAILURE;         
            
      printf("%s", tmpline);
      free(tmpline);
    } else
      printf("%s", line);

   fflush(stdout);
  } // next line

  fclose(stream);

  return MS_SUCCESS;
}

int msReturnURL(mapservObj* msObj, char* url, int mode)
{
  char *tmpurl;

  if(url == NULL) {
    msSetError(MS_WEBERR, "Empty URL.", "msReturnURL()");
    return MS_FAILURE;
//    writeError();
  }

  tmpurl = processLine(msObj, url, mode);
  redirect(tmpurl);
  free(tmpurl);
   
  return MS_SUCCESS;
}


int msReturnQuery(mapservObj* msObj)
{
  int status;
  int i,j;

  char *template;

  layerObj *lp=NULL;

  msInitShape(&(msObj->ResultShape)); // ResultShape is a global var define in mapserv.h

  if((msObj->Mode == ITEMQUERY) || (msObj->Mode == QUERY)) { // may need to handle a URL result set

    for(i=(msObj->Map->numlayers-1); i>=0; i--) {
      lp = &(msObj->Map->layers[i]);

      if(!lp->resultcache) continue;
      if(lp->resultcache->numresults > 0) break;
    }

    if(lp->class[(int)(lp->resultcache->results[0].classindex)].template) 
      template = lp->class[(int)(lp->resultcache->results[0].classindex)].template;
    else 
      template = lp->template;

    if(TEMPLATE_TYPE(template) == MS_URL) {
      msObj->ResultLayer = lp;

      status = msLayerOpen(lp, msObj->Map->shapepath);
      if(status != MS_SUCCESS)
         return status;

      // retrieve all the item names
      status = msLayerGetItems(lp);
      if(status != MS_SUCCESS)
         return status;

      status = msLayerGetShape(lp, &(msObj->ResultShape), lp->resultcache->results[0].tileindex, lp->resultcache->results[0].shapeindex);
      if(status != MS_SUCCESS)
         return status;

      if (msReturnURL(msObj, template, QUERY) != MS_SUCCESS)
           return MS_FAILURE;
      
      msFreeShape(&(msObj->ResultShape));
      msLayerClose(lp);
      msObj->ResultLayer = NULL;

      return MS_SUCCESS;
    }
  }

  msObj->NR = msObj->NL = 0;
  for(i=0; i<msObj->Map->numlayers; i++) { // compute some totals
    lp = &(msObj->Map->layers[i]);

    if(!lp->resultcache) continue;

    if(lp->resultcache->numresults > 0) { 
      msObj->NL++;
      msObj->NR += lp->resultcache->numresults;
    }
  }

  printf("Content-type: text/html%c%c", 10, 10); // write MIME header
  printf("<!-- %s -->\n", msGetVersion());
  fflush(stdout);
  
  if(msObj->Map->web.header)
     if (msReturnPage(msObj, msObj->Map->web.header, BROWSE) != MS_SUCCESS)
       return MS_FAILURE;

  msObj->RN = 1; // overall result number
  for(i=(msObj->Map->numlayers-1); i>=0; i--) {
    msObj->ResultLayer = lp = &(msObj->Map->layers[i]);

    if(!lp->resultcache) continue;
    if(lp->resultcache->numresults <= 0) continue;

    msObj->NLR = lp->resultcache->numresults; 

    if(lp->header) 
       if (msReturnPage(msObj, lp->header, BROWSE) != MS_SUCCESS)
         return MS_FAILURE;

    // open this layer
    status = msLayerOpen(lp, msObj->Map->shapepath);
    if(status != MS_SUCCESS)
       return status;

    // retrieve all the item names
    status = msLayerGetItems(lp);
    if(status != MS_SUCCESS)
       return status;

    msObj->LRN = 1; // layer result number
    for(j=0; j<lp->resultcache->numresults; j++) {
      status = msLayerGetShape(lp, &(msObj->ResultShape), lp->resultcache->results[j].tileindex, lp->resultcache->results[j].shapeindex);
      if(status != MS_SUCCESS)
         return status;
      
      if(lp->class[(int)(lp->resultcache->results[j].classindex)].template) 
	template = lp->class[(int)(lp->resultcache->results[j].classindex)].template;
      else 
	template = lp->template;

      if (msReturnPage(msObj, template, QUERY) != MS_SUCCESS)
         return MS_FAILURE;

      msFreeShape(&(msObj->ResultShape)); // init too

      msObj->RN++; // increment counters
      msObj->LRN++;
    }

    if(lp->footer) 
       if (msReturnPage(msObj, lp->footer, BROWSE) != MS_SUCCESS)
         return MS_FAILURE;

    msLayerClose(lp);
    msObj->ResultLayer = NULL;
  }

  if(msObj->Map->web.footer) 
     return msReturnPage(msObj, msObj->Map->web.footer, BROWSE);

  return MS_SUCCESS;
}

mapservObj*  msAllocMapServObj()
{
   mapservObj* msObj = malloc(sizeof(mapservObj));
   
   msObj->SaveMap=MS_FALSE;
   msObj->SaveQuery=MS_FALSE; // should the query and/or map be saved 

   msObj->ParamNames=NULL;
   msObj->ParamValues=NULL;
   msObj->NumParams=0;

   msObj->Map=NULL;

   msObj->NumLayers=0; /* number of layers specfied by a user */


   msObj->RawExt.minx=-1;
   msObj->RawExt.miny=-1;
   msObj->RawExt.maxx=-1;
   msObj->RawExt.maxy=-1;

   msObj->fZoom=1;
   msObj->Zoom=1; /* default for browsing */
   
   msObj->ResultLayer=NULL;
   

   msObj->MapPnt.x=-1;
   msObj->MapPnt.y=-1;

   msObj->ZoomDirection=0; /* whether zooming in or out, default is pan or 0 */

   msObj->Mode=BROWSE; /* can be BROWSE, QUERY, etc. */
   msObj->Id[0]='\0'; /* big enough for time + pid */

   /* 
    ** variables for multiple query results processing 
    */
   msObj->RN=0; /* overall result number */
   msObj->LRN=0; /* result number within a layer */
   msObj->NL=0; /* total number of layers with results */
   msObj->NR=0; /* total number or results */
   msObj->NLR=0; /* number of results in a layer */
   
   return msObj;
}

void msFreeMapServObj(mapservObj* msObj)
{
   free(msObj);
}

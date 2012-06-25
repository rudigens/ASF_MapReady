#include "asf_sar.h"
#include "asf_raster.h"
#include "shapefil.h"

static void open_shape(char *inFile, DBFHandle *dbase, SHPHandle *shape)
{
  char *dbaseFile;

  if (dbase) {
    dbaseFile = appendExt(inFile, ".dbf");
    *dbase = DBFOpen(dbaseFile, "r+b");
    if (*dbase == NULL)
      asfPrintError("Could not open database file '%s'\n", dbaseFile);
    FREE(dbaseFile);
  }
  if (shape) {
    char tmpInFile[1024];
    strcpy(tmpInFile, inFile);
    char *ext = findExt(inFile);
    if (!ext) {
      sprintf(tmpInFile, "%s.dummy", inFile);
    }
    *shape = SHPOpen(tmpInFile, "r+b");
    if (*shape == NULL)
      asfPrintError("Could not open shapefile '%s'\n", inFile);
  }
}

static void close_shape(DBFHandle dbase, SHPHandle shape)
{
  DBFClose(dbase);
  SHPClose(shape);
}

static void write_esri_proj_file(char *inFile)
{
  FILE *fp;
  char esri_prj_file_name[255];

  create_name (esri_prj_file_name, inFile, ".prj");
  fp = FOPEN(esri_prj_file_name, "w");
  fprintf(fp,
      "GEOGCS[\"GCS_WGS_1984\","
      "DATUM[\"D_WGS_1984\","
      "SPHEROID[\"WGS_1984\",6378137,298.257223563]],"
      "PRIMEM[\"Greenwich\",0],"
      "UNIT[\"Degree\",0.017453292519943295]]");
  FCLOSE(fp);
}

static void shape_looks_init(char *inFile)
{
  char *dbaseFile;
  DBFHandle dbase;
  SHPHandle shape;

  // Open database for initialization
  dbaseFile = appendExt(inFile, ".dbf");
  dbase = DBFCreate(dbaseFile);
  if (!dbase)
    asfPrintError("Could not create database file '%s'\n", dbaseFile);

  // Add fields to database
  if (DBFAddField(dbase, "MIN", FTDouble, 8, 3) == -1)
    asfPrintError("Could not add MIN field to database file\n");
  if (DBFAddField(dbase, "MAX", FTDouble, 8, 3) == -1)
    asfPrintError("Could not add MAX field to database file\n");
  if (DBFAddField(dbase, "MEAN", FTDouble, 10, 6) == -1)
    asfPrintError("Could not add MEAN field to database file\n");
  if (DBFAddField(dbase, "STD_DEV", FTDouble, 10, 6) == -1)
    asfPrintError("Could not add STD_DEV field to database file\n");
  if (DBFAddField(dbase, "NUM_LOOKS", FTDouble, 10, 6) == -1)
    asfPrintError("Could not add NUM_LOOKS field to database file\n");
  if (DBFAddField(dbase, "NSTART_LAT", FTDouble, 16, 4) == -1)
    asfPrintError("Could not add NSTART_LAT field to database file\n");
  if (DBFAddField(dbase, "NSTART_LON", FTDouble, 16, 4) == -1)
    asfPrintError("Could not add NSTART_LON field to database file\n");
  if (DBFAddField(dbase, "FSTART_LAT", FTDouble, 16, 4) == -1)
    asfPrintError("Could not add FSTART_LAT field to database file\n");
  if (DBFAddField(dbase, "FSTART_LON", FTDouble, 16, 4) == -1)
    asfPrintError("Could not add FSTART_LON field to database file\n");
  if (DBFAddField(dbase, "N_END_LAT", FTDouble, 16, 4) == -1)
    asfPrintError("Could not add N_END_LAT field to database file\n");
  if (DBFAddField(dbase, "N_END_LON", FTDouble, 16, 4) == -1)
    asfPrintError("Could not add N_END_LON field to database file\n");
  if (DBFAddField(dbase, "F_END_LAT", FTDouble, 16, 4) == -1)
    asfPrintError("Could not add F_END_LAT field to database file\n");
  if (DBFAddField(dbase, "F_END_LON", FTDouble, 16, 4) == -1)
    asfPrintError("Could not add F_END_LON field to database file\n");
  
  // Close the database for initialization
  DBFClose(dbase);

  // Open shapefile for initialization
  shape = SHPCreate(inFile, SHPT_POLYGON);
  if (!shape)
    asfPrintError("Could not create shapefile '%s'\n", inFile);

  // Close shapefile for initialization
  SHPClose(shape);

  FREE(dbaseFile);

  return;
}

int calc_number_looks(char *inFile, int imageFlag, int chipSize, int gisFlag)
{
  // Check for metadata file first
  char dataFile[1024], metaFile[1024], statsFile[1024];
  create_name(dataFile, inFile, ".img");
  create_name(metaFile, inFile, ".meta");
  create_name(statsFile, inFile, ".csv");
  if (!fileExists(dataFile))
    asfPrintError("Data file (%s) does not exist\n", dataFile);
  if (!fileExists(metaFile))
    asfPrintError("Metadata file (%s) does not exist\n", metaFile);
  meta_parameters *meta = meta_read(inFile);
  int line_count = meta->general->line_count;
  int sample_count = meta->general->sample_count;

  // Check for radiometry - 
  // Only possible if the data is not ingested from a GeoTIFF file
  if (strcmp_case(meta->general->sensor, "GEOTIFF") != 0) {
    if (!meta->sar)
      asfPrintError("Calc_number_looks function only works for SAR images\n");
    if (meta->general->radiometry != r_SIGMA)
      asfPrintError("Function only works on images in SIGMA power scale\n");
  }

  // Calculate the chip size (unless it is defined -
  // We want to look for homogeneous areas
  if (chipSize == MAGIC_UNSET_INT)
    chipSize = 500.0 / meta->general->x_pixel_size + 0.5;
  int size = chipSize*chipSize;
  if (imageFlag)
    size = line_count*sample_count;
  float *data = (float *) MALLOC(sizeof(float)*size);  

  // Initialize GIS file
  DBFHandle dbase;
  SHPHandle shape;
  char gisFile[1024];
  double lat[5], lon[5];
  long nChip = 0;
  if (gisFlag) {
    create_name(gisFile, inFile, ".shp");
    shape_looks_init(gisFile);
    open_shape(gisFile, &dbase, &shape);
  }

  // Calculate stats for each chip
  double min, max, mean, stdDev, nLooks, test;
  double tMin, tMax, tMean, tStdDev;
  FILE *fpIn = FOPEN(dataFile, "rb");
  FILE *fpOut = FOPEN(statsFile, "w");
  fprintf(fpOut, "min, max, mean, stdDev, nLooks\n");
  int ii, kk;
  if (imageFlag) {
    get_float_lines(fpIn, meta, 0, line_count, data);
    calc_stats(data, size, 0.0, &min, &max, &mean, &stdDev);
    nLooks = mean*mean/(stdDev*stdDev);
    fprintf(fpOut, "%.3f, %.3f, %.6f, %.6f, %.6f\n", 
	    min, max, mean, stdDev, nLooks);
  }
  else {
    for (ii=0; ii<line_count; ii+=chipSize) {
      test = 99999;
      if (ii+chipSize >= line_count)
	continue;
      for (kk=0; kk<sample_count; kk+=chipSize) {
	if (kk+chipSize >= sample_count) {
	  fprintf(fpOut, "%.3f, %.3f, %.6f, %.6f, %.6f\n", 
		  tMin, tMax, tMean, tStdDev, nLooks);
	  continue;
	}
	get_partial_float_lines(fpIn, meta, ii, chipSize, kk, chipSize, data);
	calc_stats(data, size, 0.0, &min, &max, &mean, &stdDev);
	if (gisFlag) {
	  // Determine corner coordinates of chip
	  meta_get_latLon(meta, ii, kk, 0.0, &lat[0], &lon[0]);
	  meta_get_latLon(meta, ii, kk+chipSize, 0.0, &lat[1], &lon[1]);
	  meta_get_latLon(meta, ii+chipSize, kk+chipSize, 0.0, &lat[2], &lon[2]);
	  meta_get_latLon(meta, ii+chipSize, kk, 0.0, &lat[3], &lon[3]);
	  lat[4] = lat[0];
	  lon[4] = lon[0];

	  // Write information into database file
	  DBFWriteDoubleAttribute(dbase, nChip, 0, min);
	  DBFWriteDoubleAttribute(dbase, nChip, 1, max);
	  DBFWriteDoubleAttribute(dbase, nChip, 2, mean);
	  DBFWriteDoubleAttribute(dbase, nChip, 3, stdDev);
	  DBFWriteDoubleAttribute(dbase, nChip, 4, nLooks);
	  DBFWriteDoubleAttribute(dbase, nChip, 5, lat[0]);
	  DBFWriteDoubleAttribute(dbase, nChip, 6, lon[0]);
	  DBFWriteDoubleAttribute(dbase, nChip, 7, lat[1]);
	  DBFWriteDoubleAttribute(dbase, nChip, 8, lon[1]);
	  DBFWriteDoubleAttribute(dbase, nChip, 9, lat[2]);
	  DBFWriteDoubleAttribute(dbase, nChip, 10, lon[2]);
	  DBFWriteDoubleAttribute(dbase, nChip, 11, lat[3]);
	  DBFWriteDoubleAttribute(dbase, nChip, 12, lon[3]);
	  
	  // Write shape object
	  SHPObject *shapeObject=NULL;
	  shapeObject = SHPCreateSimpleObject(SHPT_POLYGON, 5, lon, lat, NULL);
	  if (shapeObject == NULL)
	    asfPrintError("Could not create shape object (%d)\n", nChip);
	  SHPWriteObject(shape, -1, shapeObject);
	  SHPDestroyObject(shapeObject);
	  nChip++;
	}
	if (mean > 0.01) {
	  if ((stdDev/mean) < test) {
	    test = stdDev/mean;
	    tMin = min;
	    tMax = max;
	    tMean = mean;
	    tStdDev = stdDev;
	    nLooks = mean*mean/(stdDev*stdDev);
	  }
	}
      }
    }
  }
  FCLOSE(fpIn);
  FCLOSE(fpOut);
  meta_free(meta);
  FREE(data);
  if (!imageFlag)
    asfPrintStatus("chip size: %d\n", chipSize);
  if (gisFlag) {
    close_shape(dbase, shape);
    write_esri_proj_file(gisFile);
  }

  return TRUE;
}

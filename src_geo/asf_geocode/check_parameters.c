// standard libraries
#include <math.h>

// libraries developed by ASF
#include <asf_meta.h>
#include <asf_raster.h>
#include <asf_reporting.h>

// Prototypes
int calc_utm_zone(double lon);
void check_parameters(projection_type_t projection_type, 
		      project_parameters_t *pp, meta_parameters *meta);

// Checking routine for projection parameter input.
void check_parameters(projection_type_t projection_type, 
		      project_parameters_t *pp, meta_parameters *meta)
{
  double lat, lon;
  int zone, min_zone=60, max_zone=1;

  switch (projection_type)
    {
    case UNIVERSAL_TRANSVERSE_MERCATOR:
      // Debugging print
      printf("Projection: UTM\nZone: %i\nLatitude of origin: %.4f\n"
	     "Central meridian: %.4f\nFalse easting: %.0f\nFalse northing: %.0f\n"
	     "Scale: %.4f\n\n", pp->utm.zone, pp->utm.lat0, pp->utm.lon0, 
	     pp->utm.false_easting, pp->utm.false_northing, pp->utm.scale_factor);

      // Outside range tests
      if (pp->utm.zone < 1 || pp->utm.zone > 60)
	asfPrintError("Zone '%i' outside the defined range (1 to 60)\n", pp->utm.zone);
      if (pp->utm.lat0 < -90 || pp->utm.lat0 > 90)
	asfPrintError("Central meridian '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->utm.lat0);
      if (pp->utm.lon0 < -180 || pp->utm.lon0 > 180)
	asfPrintError("Latitude of origin '%.4f' outside the defined range "
		      "(-180 deg to 180 deg)\n", pp->utm.lon0);
      if (!FLOAT_EQUIVALENT(pp->utm.scale_factor, 0.9996))
	asfPrintError("Scale factor '%.4f' different from default value (0.9996)\n", 
		      pp->utm.scale_factor);
      if (meta->general->center_latitude >= 0.0 &&
          !FLOAT_EQUIVALENT(pp->utm.false_easting, 500000))
        asfPrintError("False easting '%.1f' different from default value (500000)\n",
                      pp->utm.false_easting);
      if (meta->general->center_latitude >= 0.0 &&
          !FLOAT_EQUIVALENT(pp->utm.false_northing, 0))
        asfPrintError("False northing '%.1f' different from default value (0)\n",
                      pp->utm.false_easting);
      if (meta->general->center_latitude < 0.0 &&
          !FLOAT_EQUIVALENT(pp->utm.false_easting, 500000))
        asfPrintError("False easting '%.1f' different from default value (500000)\n",
                      pp->utm.false_easting);
      if (meta->general->center_latitude < 0.0 &&
          !FLOAT_EQUIVALENT(pp->utm.false_northing, 10000000))
        asfPrintError("False northing '%.1f' different from default value (10000000)\n",
                      pp->utm.false_easting);

      // Zone test - only zones for coordinates within image permitted
      meta_get_latLon(meta, 0, 0, 0.0, &lat, &lon);
      zone = calc_utm_zone(lon);
      if (zone < min_zone) min_zone = zone;
      if (zone > max_zone) max_zone = zone;
      meta_get_latLon(meta, 0, meta->general->sample_count, 0.0, &lat, &lon);
      zone = calc_utm_zone(lon);
      if (zone < min_zone) min_zone = zone;
      if (zone > max_zone) max_zone = zone;
      meta_get_latLon(meta, meta->general->line_count, 0, 0.0, &lat, &lon);
      zone = calc_utm_zone(lon);
      if (zone < min_zone) min_zone = zone;
      if (zone > max_zone) max_zone = zone;
      meta_get_latLon(meta, meta->general->line_count, meta->general->sample_count,
                      0.0, &lat, &lon);
      zone = calc_utm_zone(lon);
      if (zone < min_zone) min_zone = zone;
      if (zone > max_zone) max_zone = zone;
      if (pp->utm.zone < min_zone || pp->utm.zone > max_zone)
        asfPrintError("Zone '%i' outside the range of corresponding image coordinates "
                      "(%i to %i)\n", pp->utm.zone, min_zone, max_zone);

      break;

    case POLAR_STEREOGRAPHIC:
      // Debugging print
      printf("Projection: Polar Stereographic\nStandard parallel: %.4f\n"
	     "Central meridian: %.4f\nHemisphere: %i\nFalse easting: %.0f\n"
	     "False northing: %.0f\n", pp->ps.slat, pp->ps.slon, pp->ps.is_north_pole,
	     pp->ps.false_easting, pp->ps.false_northing);

      // Outside range tests
      if (pp->ps.slat < -90 || pp->ps.slat > 90)
	asfPrintError("Central meridian '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->ps.slat);
      if (pp->ps.slon < -180 || pp->ps.slon > 180)
	asfPrintError("Latitude of origin '%.4f' outside the defined range "
		      "(-180 deg to 180 deg)\n", pp->ps.slon);

      // Distortion test - only areas with a latitude above 60 degrees North or 
      // below -60 degrees South are permitted
      if (meta->general->center_latitude < 60.0 && pp->ps.is_north_pole)
        asfPrintError("Geocoding of areas below 60 degrees latitude in the "
                      "polar stereographic map projection is not supported "
                      "by this tool\n");
      if (meta->general->center_latitude > -60.0 && !pp->ps.is_north_pole)
        asfPrintError("Geocoding of areas above -60 degrees latitude in the "
                      "polar stereographic map projection is not supported "
                      "by this tool\n");

      break;

    case ALBERS_EQUAL_AREA:
      // Debugging print
      printf("Projection: Albert Equal Area Conic\nFirst standard parallel: %.4f\n"
	     "Second standard parallel: %.4f\nCentral meridian: %.4f\n"
	     "Latitude of origin: %.4f\nFalse easting: %.0f\nFalse northing: %.0f\n",
	     pp->albers.std_parallel1, pp->albers.std_parallel2, 
	     pp->albers.center_meridian, pp->albers.orig_latitude,
	     pp->albers.false_easting, pp->albers.false_northing);

      // Outside range tests
      if (pp->albers.std_parallel1 < -90 || pp->albers.std_parallel1 > 90)
	asfPrintError("First standard parallel '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->albers.std_parallel1);
      if (pp->albers.std_parallel2 < -90 || pp->albers.std_parallel2 > 90)
	asfPrintError("Second standard parallel '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->albers.std_parallel2);
      if (pp->albers.center_meridian < -180 || pp->albers.center_meridian > 180)
	asfPrintError("Central meridian '%.4f' outside the defined range "
		      "(-180 deg to 180 deg)\n", pp->albers.center_meridian);
      if (pp->albers.orig_latitude < -90 || pp->albers.orig_latitude > 90)
	asfPrintError("Latitude of origin '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->albers.orig_latitude);

      // This projection should be used for predominantly East-West oriented
      // areas. The latitude range should not exceed 30 to 35 degrees. These
      // restrictions do not really apply to geocoding regular radar imagery.

      break;

    case LAMBERT_CONFORMAL_CONIC:
      // Debugging print
      printf("Projection: Albert Equal Area Conic\nFirst standard parallel: %.4f\n"
	     "Second standard parallel: %.4f\nCentral meridian: %.4f\n"
	     "Latitude of origin: %.4f\nFalse easting: %.0f\nFalse northing: %.0f\n",
	     pp->lamcc.plat1, pp->lamcc.plat2, pp->lamcc.lon0, pp->lamcc.lat0,
	     pp->lamcc.false_easting, pp->lamcc.false_northing);

      // Outside range tests
      if (pp->lamcc.plat1 < -90 || pp->lamcc.plat1 > 90)
	asfPrintError("First standard parallel '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->lamcc.plat1);
      if (pp->lamcc.plat2 < -90 || pp->lamcc.plat2 > 90)
	asfPrintError("Second standard parallel '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->lamcc.plat2);
      if (pp->lamcc.lon0 < -180 || pp->lamcc.lon0 > 180)
	asfPrintError("Central meridian '%.4f' outside the defined range "
		      "(-180 deg to 180 deg)\n", pp->lamcc.lon0);
      if (pp->lamcc.lat0 < -90 || pp->lamcc.lat0 > 90)
	asfPrintError("Latitude of origin '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->lamcc.lat0);
    
      break;
    case LAMBERT_AZIMUTHAL_EQUAL_AREA:
      // Debugging print
      printf("Projection: Lambert Azimuthal Equal Area\nLatitude of origin: %.4f\n"
	     "Central meridian: %.4f\nFalse easting: %.0f\nFalse northing: %.0f\n",
	     pp->lamaz.center_lat, pp->lamaz.center_lon, 
	     pp->utm.false_easting, pp->utm.false_northing);

      // Outside range tests
      if (pp->lamaz.center_lon < -180 || pp->lamaz.center_lon > 180)
	asfPrintError("Central meridian '%.4f' outside the defined range "
		      "(-180 deg to 180 deg)\n", pp->lamaz.center_lon);
      if (pp->lamaz.center_lat < -90 || pp->lamaz.center_lat > 90)
	asfPrintError("Latitude of origin '%.4f' outside the defined range "
		      "(-90 deg to 90 deg)\n", pp->lamaz.center_lat);

      break;

    default:
      asfPrintError("Chosen projection type not supported!\n");
      break;
    }
}


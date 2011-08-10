/*
* Copyright 2011 Anders Ma (andersma.net). All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. The name of the copyright holder may not be used to endorse or promote
* products derived from this software without specific prior written
* permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL WILLIAM TISÄTER BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>

#include "amcc.h"
#include "attitude.h"
#include "math.h"

/* default paramenters */
#define PIE 3.1415926f
#define ACC_ONEG "800" // default in 1.5G mode, 800mv
#define GYRO_ONEDPS "6.7" // default one degree/second is 6.7mv
#define ACC_0G_VOLTAGE "1650" // zero g voltage (mv)
#define ACC_1G_VOLTAGE "2450" // zero g voltage (mv)
#define GYRO_0DS_VOLTAGE "1800" // 0degree/s voltage (mv)

/* xml file */
#define ATTITUDE_XML_FILENAME "attitude.xml"
#define ACC_NODE "accelerometer"
#define GYRO_NODE "gyroscope"
#define AXES_X_NORMAL_VOLTAGE_NODE "normalX"
#define AXES_Y_NORMAL_VOLTAGE_NODE "normalY"
#define AXES_Z_NORMAL_VOLTAGE_NODE "normalZ"
#define ACC_ONEG_NODE "oneG"
#define GYRO_ONE_DPS_NODE "oneDPS"

float pitch_angle, roll_angle, yaw_angle;

static int write_default_attitude_xml()
{
	xmlDocPtr doc = NULL;       /* document pointer */
	xmlNodePtr attitude_node, sensors_node = NULL;	/* node pointers */
	xmlNodePtr acc_node = NULL, gyro_node = NULL;	/* node pointers */
	xmlNodePtr property_node = NULL;	/* node pointers */
	xmlDtdPtr dtd = NULL;       /* DTD pointer */

	LIBXML_TEST_VERSION;

	/* 
	 * Creates a new document, a node and set it as a root node
	 */
	doc = xmlNewDoc(BAD_CAST "1.0");
	attitude_node = xmlNewNode(NULL, BAD_CAST "attitude");
	xmlDocSetRootElement(doc, attitude_node);

	/*
	 * Creates a DTD declaration. Isn't mandatory. 
	 */
	dtd = xmlCreateIntSubset(doc, BAD_CAST "sensors", NULL, BAD_CAST "tree2.dtd");

	/*
	 * Sensors data
	 */
	sensors_node = xmlNewChild(attitude_node, NULL, BAD_CAST "sensors", NULL);
	// Accelerometer
	acc_node = xmlNewChild(sensors_node, NULL, BAD_CAST ACC_NODE, NULL);
	property_node = xmlNewChild(acc_node, NULL, BAD_CAST "property",
						BAD_CAST ACC_0G_VOLTAGE);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST AXES_X_NORMAL_VOLTAGE_NODE);
	property_node = xmlNewChild(acc_node, NULL, BAD_CAST "property",
						BAD_CAST ACC_0G_VOLTAGE);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST AXES_Y_NORMAL_VOLTAGE_NODE);
	property_node = xmlNewChild(acc_node, NULL, BAD_CAST "property",
						BAD_CAST ACC_0G_VOLTAGE);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST AXES_Z_NORMAL_VOLTAGE_NODE);
	property_node = xmlNewChild(acc_node, NULL, BAD_CAST "property",
						BAD_CAST ACC_ONEG);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST ACC_ONEG_NODE);
	// Gyroscope
	gyro_node = xmlNewChild(sensors_node, NULL, BAD_CAST GYRO_NODE, NULL);
	property_node = xmlNewChild(gyro_node, NULL, BAD_CAST "property",
						BAD_CAST GYRO_0DS_VOLTAGE);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST AXES_X_NORMAL_VOLTAGE_NODE);
	property_node = xmlNewChild(gyro_node, NULL, BAD_CAST "property",
						BAD_CAST GYRO_0DS_VOLTAGE);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST AXES_Y_NORMAL_VOLTAGE_NODE);
	property_node = xmlNewChild(gyro_node, NULL, BAD_CAST "property",
						BAD_CAST GYRO_0DS_VOLTAGE);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST AXES_Z_NORMAL_VOLTAGE_NODE);
	property_node = xmlNewChild(gyro_node, NULL, BAD_CAST "property",
						BAD_CAST GYRO_ONEDPS);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST GYRO_ONE_DPS_NODE);

	/* 
	 * Dumping document to stdio or file
	 */
	xmlSaveFormatFileEnc(ATTITUDE_XML_FILENAME, doc, "UTF-8", 1);

	/*free the document */
	xmlFreeDoc(doc);

	/*
	 *Free the global variables that may
	 *have been allocated by the parser.
	*/
	xmlCleanupParser();

	/*
	 * this is to debug memory for regression tests
	 */
	xmlMemoryDump();
	return(0);
}

static int init_attitude_from_xml(attitude_t *atd)
{
/*
	Get the node type of the current node Reference:
	http://www.gnu.org/software/dotgnu/pnetlib-doc/System/Xml/XmlNodeType.html
*/	
	xmlTextReaderPtr reader;
	const xmlChar *key, *name, *value;
	int ret, parent_node = 0;

	if (access(ATTITUDE_XML_FILENAME, F_OK) != 0) {
		write_default_attitude_xml();
	}

	reader = xmlReaderForFile(ATTITUDE_XML_FILENAME, NULL, 0);
	if (reader != NULL) {
		ret = xmlTextReaderRead(reader);
		while (ret == 1) {
			/* 
			 *parse XML node 
			 */
			name = xmlTextReaderConstName(reader);
			if (name == NULL)
				name = BAD_CAST "--";
			// mark parent node
			if (strcmp(ACC_NODE, name) == 0) {
				parent_node = 1;
			} else if (strcmp(GYRO_NODE, name) == 0) {
				parent_node = 2;
			}
			// check key
			key = xmlTextReaderGetAttribute(reader, BAD_CAST "key");
			if (key == NULL) {
				ret = xmlTextReaderRead(reader);
				continue;
			} else {
				if (xmlTextReaderNodeType(reader) == 1) { // XmlNodeType.Element Field
					while (ret == 1) {
						ret = xmlTextReaderRead(reader);
						if (xmlTextReaderNodeType(reader) == 3) // XmlNodeType.Text Field
							break;
						continue;
					}
					if (ret != 1)
						break;
				} else {
					ret = xmlTextReaderRead(reader);
					continue;
				}
			}
			value = xmlTextReaderConstValue(reader);
			if(name) {
				value = xmlTextReaderConstValue(reader);
				// mapping data
				if (strcmp(AXES_X_NORMAL_VOLTAGE_NODE, key) == 0) {
					switch (parent_node) {
					case 1: // Acc
						atd->acc_nml_x = atoi(value);
						break;
					case 2: // Gyro
						atd->gyro_nml_x = atoi(value);
						break;
					}
				} else if (strcmp(AXES_Y_NORMAL_VOLTAGE_NODE, key) == 0) {
					switch (parent_node) {
					case 1: // Acc
						atd->acc_nml_y = atoi(value);
						break;
					case 2: // Gyro
						atd->gyro_nml_y = atoi(value);
						break;
					}
				} else if (strcmp(AXES_Z_NORMAL_VOLTAGE_NODE, key) == 0) {
					switch (parent_node) {
					case 1: // Acc
						atd->acc_nml_z = atoi(value);
						break;
					case 2: // Gyro
						atd->gyro_nml_z = atoi(value);
						break;
					}
				} else if (strcmp(ACC_ONEG_NODE, key) == 0) {
					atd->acc_1g = atoi(value);
				} else if (strcmp(GYRO_ONE_DPS_NODE, key) == 0) {
					atd->gyro_dps = atof(value);
				}
			}
			ret = xmlTextReaderRead(reader);
		}
		xmlFreeTextReader(reader);
		if (ret != 0) {
			fprintf(stderr, "%s : failed to parse\n", ATTITUDE_XML_FILENAME);
			return -1;
		}
	} else {
		fprintf(stderr, "Unable to open %s\n", ATTITUDE_XML_FILENAME);
		return -1;
	}
/*
	printf("%d\n", atd->acc_nml_x);
	printf("%d\n", atd->acc_nml_y);
	printf("%d\n", atd->acc_nml_z);
	printf("%d\n", atd->gyro_nml_x);
	printf("%d\n", atd->gyro_nml_y);
	printf("%d\n", atd->gyro_nml_z);
	printf("%d\n", atd->acc_1g);
	printf("%f\n", atd->gyro_dps);
*/
	return 0;
}

/*
 * read sensor's configuration from XML file
 */
void attitude_init(attitude_t *atd)
{
	init_attitude_from_xml(atd);
}

void attitude_by_acc(attitude_t *atd)
{
	double voltage, accx, accy, accz;

	voltage = (atd->acc_crt_x * 3300) / 4096.0;
	accx = (voltage - atd->acc_nml_x) / atd->acc_1g;
	voltage = (atd->acc_crt_y * 3300) / 4096.0;
	accy = (voltage - atd->acc_nml_y) / atd->acc_1g;
	voltage = (atd->acc_crt_z * 3300) / 4096.0;
	accz = (voltage - atd->acc_nml_z + atd->acc_1g) / atd->acc_1g;
/*
	accx = (double)(atd->acc_crt_x - atd->acc_nml_x);
	accy = (double)(atd->acc_crt_y - atd->acc_nml_y);
	accz = (double)(atd->acc_crt_z - atd->acc_nml_z + atd->acc_1g);
*/
	roll_angle = atan(accy / accz) * (360 / PIE);
	pitch_angle = atan(accx / (sqrt( accy * accy + accz * accz))) * (360 / PIE);

}

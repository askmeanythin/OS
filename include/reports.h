/* reports.h - exam reports and CSV exports generated from stored data */
#ifndef SEL_REPORTS_H
#define SEL_REPORTS_H

#include "common.h"

int reports_write_exam_report(const char *exam_id, const char *out_path); /* text report */
int reports_export_computers_csv(const char *out_path);
int reports_export_assignments_csv(const char *exam_id, const char *out_path);
int reports_export_alerts_csv(const char *out_path);
void csv_escape(FILE *f, const char *s); /* writes one escaped CSV field */

#endif

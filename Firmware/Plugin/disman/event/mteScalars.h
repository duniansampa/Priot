#ifndef MTESCALARS_H
#define MTESCALARS_H

#include "AgentHandler.h"

void            init_mteScalars(void);
NodeHandlerFT handle_mteResourceGroup;
NodeHandlerFT handle_mteTriggerFailures;

#define	MTE_RESOURCE_SAMPLE_MINFREQ		1
#define	MTE_RESOURCE_SAMPLE_MAX_INST		2
#define	MTE_RESOURCE_SAMPLE_INSTANCES		3
#define	MTE_RESOURCE_SAMPLE_HIGH		4
#define	MTE_RESOURCE_SAMPLE_LACKS		5

#endif                          /* MTESCALARS_H */

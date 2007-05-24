/*

    $Id: sopen_hl7aecg.c,v 1.5 2007-05-24 10:10:32 schloegl Exp $
    Copyright (C) 2006,2007 Alois Schloegl <a.schloegl@ieee.org>
    Copyright (C) 2007 Elias Apostolopoulos
    This function is part of the "BioSig for C/C++" repository 
    (biosig4c++) at http://biosig.sf.net/ 


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 */




#include <stdio.h>             // system includes
#include <vector>
#include <string>
#include <sstream>
//#include <libxml/tree.h>
//#include <libxml/xmlreader.h>

#include "biosig.h"
#include "XMLParser/tinyxml.h"
#include "XMLParser/Tokenizer.h"

HDRTYPE* sopen_HL7aECG_read(HDRTYPE* hdr){
/*
	this function is a stub or placeholder and need to be defined in order to be useful.
	It will be called by the function SOPEN in "biosig.c"

	Input: 
		char* Header1	// contains the file content
		
	Output: 
		HDRTYPE *hdr	// defines the HDR structure accoring to "biosig.h"
*/
	TiXmlDocument doc(hdr->FileName);
	if(doc.LoadFile()){
	    TiXmlHandle hDoc(&doc);
	    TiXmlHandle aECG = hDoc.FirstChild("AnnotatedECG");
	    if(aECG.Element()){
		hdr->AS.RID = strdup(aECG.FirstChild("id").Element()->Attribute("root"));

		TiXmlHandle effectiveTime = aECG.FirstChild("effectiveTime");

		char *T0 = NULL;

		if(effectiveTime.FirstChild("low").Element())
		    T0 = (char *)effectiveTime.FirstChild("low").Element()->Attribute("value");
		else if(effectiveTime.FirstChild("center").Element())
		    T0 = (char *)effectiveTime.FirstChild("center").Element()->Attribute("value");

		struct tm *t0 = (struct tm *)malloc(sizeof(struct tm));
		T0[14] = '\0';
		t0->tm_sec = atoi(T0+12);
		T0[12] = '\0';
		t0->tm_min = atoi(T0+10);
		T0[10] = '\0';
		t0->tm_hour = atoi(T0+8);
		T0[8] = '\0';
		t0->tm_mday = atoi(T0+6);
		T0[6] = '\0';
		t0->tm_mon = atoi(T0+4)-1;
		T0[4] = '\0';
		t0->tm_year = atoi(T0)-1900;

 		hdr->T0 = tm_time2gdf_time(t0);

		TiXmlHandle demographic = aECG.FirstChild("componentOf").FirstChild("timepointEvent").FirstChild("componentOf").FirstChild("subjectAssignment").FirstChild("subject").FirstChild("trialSubject");

		TiXmlElement *id = demographic.FirstChild("id").Element();
		if(id)
		    hdr->Patient.Id = strdup(id->Attribute("extension"));
		
		demographic = demographic.FirstChild("subjectDemographicPerson");

		TiXmlElement *name = demographic.FirstChild("name").Element();
		if(name)
		    hdr->Patient.Name = strdup(name->GetText());
		else
		    fprintf(stderr,"Error: name\n");
		
		TiXmlElement *birthday = demographic.FirstChild("birthTime").Element();
		if(birthday){
		    T0 = (char *)birthday->Attribute("value");
		    T0[14] = '\0';
		    t0->tm_sec = atoi(T0+12);
		    T0[12] = '\0';
		    t0->tm_min = atoi(T0+10);
		    T0[10] = '\0';
		    t0->tm_hour = atoi(T0+8);
		    T0[8] = '\0';
		    t0->tm_mday = atoi(T0+6);
		    T0[6] = '\0';
		    t0->tm_mon = atoi(T0+4)-1;
		    T0[4] = '\0';
		    t0->tm_year = atoi(T0)-1900;

		    hdr->Patient.Birthday = tm_time2gdf_time(t0);
		}
		else
		    fprintf(stderr,"Error: birthday\n");
		
		TiXmlElement *sex = demographic.FirstChild("administrativeGenderCode").Element();
		if(sex){
		    if(!strcmp(sex->Attribute("code"),"F"))
			hdr->Patient.Sex = 2;
		    else if(!strcmp(sex->Attribute("code"),"M"))
			hdr->Patient.Sex = 1;
		    else
			hdr->Patient.Sex = 0;
		}else{
		    hdr->Patient.Sex = 0;
		}
		
		int LowPass=0, HighPass=0, Notch=0;
		TiXmlHandle channels = aECG.FirstChild("component").FirstChild("series").FirstChild("component").FirstChild("sequenceSet");
		TiXmlHandle variables = aECG.FirstChild("component").FirstChild("series");

		for(TiXmlElement *tmpvar = variables.FirstChild("controlVariable").Element(); tmpvar; tmpvar = tmpvar->NextSiblingElement("controlVariable")){
		    if(!strcmp(tmpvar->FirstChildElement("controlVariable")->FirstChildElement("code")->Attribute("code"), "MDC_ATTR_FILTER_NOTCH"))
			Notch = atoi(tmpvar->FirstChildElement("controlVariable")->FirstChildElement("component")->FirstChildElement("controlVariable")->FirstChildElement("value")->Attribute("value"));
		    else if(!strcmp(tmpvar->FirstChildElement("controlVariable")->FirstChildElement("code")->Attribute("code"), "MDC_ATTR_FILTER_LOW_PASS"))
			LowPass = atoi(tmpvar->FirstChildElement("controlVariable")->FirstChildElement("component")->FirstChildElement("controlVariable")->FirstChildElement("value")->Attribute("value"));
		    else if(!strcmp(tmpvar->FirstChildElement("controlVariable")->FirstChildElement("code")->Attribute("code"), "MDC_ATTR_FILTER_HIGH_PASS"))
			HighPass = atoi(tmpvar->FirstChildElement("controlVariable")->FirstChildElement("component")->FirstChildElement("controlVariable")->FirstChildElement("value")->Attribute("value"));
		}
		hdr->NRec = 1;
		hdr->SPR = 1;

		hdr->AS.rawdata = (uint8_t *)malloc(hdr->SPR);
		int16_t *data;
		
		hdr->SampleRate = atof(channels.FirstChild("component").FirstChild("sequence").FirstChild("value").FirstChild("increment").Element()->Attribute("value"));
		
		TiXmlHandle channel = channels.Child("component", 1).FirstChild("sequence");
		for(hdr->NS = 0; channel.Element(); ++(hdr->NS), channel = channels.Child("component", hdr->NS+1).FirstChild("sequence"));
		hdr->CHANNEL = (CHANNEL_TYPE*) calloc(hdr->NS,sizeof(CHANNEL_TYPE));

		channel = channels.Child("component", 1).FirstChild("sequence");
		for(int i = 0; channel.Element(); ++i, channel = channels.Child("component", i+1).FirstChild("sequence")){

		    const char *code = channel.FirstChild("code").Element()->Attribute("code");
		    
		    hdr->CHANNEL[i].Label = strdup(code);

		    if(!strcmp(code+13, "I"))
			hdr->CHANNEL[i].LeadIdCode = 1;
		    else if(!strcmp(code+13, "II"))
			hdr->CHANNEL[i].LeadIdCode = 2;
		    else if(!strcmp(code+13, "III"))
			hdr->CHANNEL[i].LeadIdCode = 61;
		    else if(!strcmp(code+13, "AVR"))
			hdr->CHANNEL[i].LeadIdCode = 62;
		    else if(!strcmp(code+13, "AVL"))
			hdr->CHANNEL[i].LeadIdCode = 63;
		    else if(!strcmp(code+13, "AVF"))
			hdr->CHANNEL[i].LeadIdCode = 64;
		    else if(!strcmp(code+13, "V1"))
			hdr->CHANNEL[i].LeadIdCode = 3;
		    else if(!strcmp(code+13, "V2"))
			hdr->CHANNEL[i].LeadIdCode = 4;
		    else if(!strcmp(code+13, "V3"))
			hdr->CHANNEL[i].LeadIdCode = 5;
		    else if(!strcmp(code+13, "V4"))
			hdr->CHANNEL[i].LeadIdCode = 6;
		    else if(!strcmp(code+13, "V5"))
			hdr->CHANNEL[i].LeadIdCode = 7;
		    else if(!strcmp(code+13, "V6"))
			hdr->CHANNEL[i].LeadIdCode = 8;

		    hdr->CHANNEL[i].Transducer = "EEG: Ag-AgCl electrodes";
//		    hdr->CHANNEL[k].Transducer = (Header2 + 16*hdr->NS + 80*k);

		    hdr->CHANNEL[i].GDFTYP = 3;	// int16

		    std::vector<std::string> vector;
		    stringtokenizer(vector, channel.FirstChild("value").FirstChild("digits").Element()->GetText());

		    hdr->CHANNEL[i].SPR = vector.size();
		    hdr->SPR = lcm(hdr->SPR, hdr->CHANNEL[i].SPR);
		    hdr->AS.rawdata = (uint8_t *)realloc(hdr->AS.rawdata, 2*(i+1)*hdr->NS*hdr->SPR*hdr->NRec);

		    data = (int16_t *)hdr->AS.rawdata+(i*hdr->SPR);

		    hdr->CHANNEL[i].Cal  = atof(channel.FirstChild("value").FirstChild("scale").Element()->Attribute("value"));
		    hdr->CHANNEL[i].Off  = atof(channel.FirstChild("value").FirstChild("origin").Element()->Attribute("value"));
		    
		    for(unsigned int j=0; j<hdr->SPR; ++j){
			data[j] = atoi(vector[j].c_str());
			if(data[j] > hdr->CHANNEL[i].DigMax){
			    hdr->CHANNEL[i].DigMax = data[j];
			}
			if(data[j] < hdr->CHANNEL[i].DigMin){
			    hdr->CHANNEL[i].DigMin = data[j];
			}
		    	hdr->CHANNEL[i].PhysMax = hdr->CHANNEL[i].DigMax*hdr->CHANNEL[i].Cal + hdr->CHANNEL[i].Off;
		    	hdr->CHANNEL[i].PhysMin = hdr->CHANNEL[i].DigMin*hdr->CHANNEL[i].Cal + hdr->CHANNEL[i].Off;
		    }
		    
		    hdr->CHANNEL[i].PhysDim = "uV";           //Hardcoded
		    hdr->CHANNEL[i].PhysDimCode = 4256+19;    //Hardcoded

		    hdr->CHANNEL[i].LowPass  = LowPass;
		    hdr->CHANNEL[i].HighPass = HighPass;
		    hdr->CHANNEL[i].Notch    = Notch;
// 			hdr->CHANNEL[i].XYZ[0]   = l_endian_f32( *(float*) (Header2+ 4*k + 224*hdr->NS) );
// 			hdr->CHANNEL[i].XYZ[1]   = l_endian_f32( *(float*) (Header2+ 4*k + 228*hdr->NS) );
// 			hdr->CHANNEL[i].XYZ[2]   = l_endian_f32( *(float*) (Header2+ 4*k + 232*hdr->NS) );
// 				//memcpy(&hdr->CHANNEL[k].XYZ,Header2 + 4*k + 224*hdr->NS,12);
// 			hdr->CHANNEL[i].Impedance= ldexp(1.0, (uint8_t)Header2[k + 236*hdr->NS]/8);

//		    hdr->CHANNEL[i].Impedance = INF;
//		    for(int k1=0; k1<3; hdr->CHANNEL[index].XYZ[k1++] = 0.0);
		}
		hdr->SampleRate *= hdr->SPR;
		hdr->SampleRate = hdr->SPR/hdr->SampleRate;
		
		hdr->FLAG.OVERFLOWDETECTION = 0;
	    }else{
		fprintf(stderr, "%s : failed to parse\n", hdr->FileName);
	    }
	}
	else
	    fprintf(stderr, "%s : failed to parse\n", hdr->FileName);

	return(hdr);

};


HDRTYPE* sopen_HL7aECG_write(HDRTYPE* hdr){
/*
	this function is a stub or placeholder and need to be defined in order to be useful. 
	It will be called by the function SOPEN in "biosig.c"

	Input: HDR structure
		
	Output: 
		char* HDR.AS.Header1 	// contains the content which will be written to the file in SOPEN
*/	
    TiXmlDocument doc;
    
    TiXmlDeclaration* decl = new TiXmlDeclaration("1.0", "UTF-8", "");
    doc.LinkEndChild(decl);
    
    TiXmlElement *root = new TiXmlElement("AnnotatedECG");
    root->SetAttribute("xmlns", "urn:hl7-org:v3");
    root->SetAttribute("xmlns:voc", "urn:hl7-org:v3/voc");
    root->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root->SetAttribute("xsi:schemaLocation", "urn:hl7-org:v3/HL7/aECG/2003-12/schema/PORT_MT020001.xsd");
    root->SetAttribute("classCode", "OBS");
    root->SetAttribute("moodCode", "EVN");
    doc.LinkEndChild(root);
    
    TiXmlElement *rootid = new TiXmlElement("id");
    rootid->SetAttribute("root", hdr->AS.RID);
    root->LinkEndChild(rootid);
	
    TiXmlElement *rootCode = new TiXmlElement("code");
    rootCode->SetAttribute("code", "93000");
    rootCode->SetAttribute("codeSystem", "2.16.840.1.113883.6.12");
    rootCode->SetAttribute("codeSystemName", "CPT-4");
    root->LinkEndChild(rootCode);
    
    char timelow[19], timehigh[19];
    time_t T0 = gdf_time2t_time(hdr->T0);
    struct tm *t0 = localtime(&T0);
    sprintf(timelow, "%4d%2d%2d%2d%2d%2d.000", t0->tm_year+1900, t0->tm_mon+1, t0->tm_mday, t0->tm_hour, t0->tm_min, t0->tm_sec);
    sprintf(timehigh, "%4d%2d%2d%2d%2d%2d.000", t0->tm_year+1900, t0->tm_mon+1, t0->tm_mday, t0->tm_hour, t0->tm_min, t0->tm_sec+hdr->SPR/((int)hdr->SampleRate));
    for(int i=0; i<18; ++i){
	if(timelow[i] == ' ')
	    timelow[i] = '0';
	if(timehigh[i] == ' ')
	    timehigh[i] = '0';
    }

    TiXmlElement *effectiveTime = new TiXmlElement("effectiveTime");
    TiXmlElement *effectiveTimeLow = new TiXmlElement("low");
    effectiveTimeLow->SetAttribute("value", timelow);
    effectiveTime->LinkEndChild(effectiveTimeLow);
    TiXmlElement *effectiveTimeHigh = new TiXmlElement("high");
    effectiveTimeHigh->SetAttribute("value", timehigh);
    effectiveTime->LinkEndChild(effectiveTimeHigh);
    root->LinkEndChild(effectiveTime);

    TiXmlElement *rootComponentOf = new TiXmlElement("componentOf");
    rootComponentOf->SetAttribute("typeCode", "COMP");
    rootComponentOf->SetAttribute("contextConductionInd", "true");
    root->LinkEndChild(rootComponentOf);
    
    TiXmlElement *timePointEvent = new TiXmlElement("timePointEvent");
    timePointEvent->SetAttribute("classCode", "CTTEVENT");
    timePointEvent->SetAttribute("moodCode", "EVN");
    rootComponentOf->LinkEndChild(timePointEvent);
    
    TiXmlElement *timePointComponentOf = new TiXmlElement("componentOf");
    timePointComponentOf->SetAttribute("typeCode", "COMP");
    timePointComponentOf->SetAttribute("contextConductionInd", "true");
    timePointEvent->LinkEndChild(timePointComponentOf);

    TiXmlElement *subjectAssignment = new TiXmlElement("subjectAssignment");
    subjectAssignment->SetAttribute("classCode", "CLNTRL");
    subjectAssignment->SetAttribute("moodCode", "EVN");
    timePointComponentOf->LinkEndChild(subjectAssignment);

    TiXmlElement *subject = new TiXmlElement("subject");
    subject->SetAttribute("typeCode", "SBJ");
    subject->SetAttribute("contextControlCode", "OP");
    subjectAssignment->LinkEndChild(subject);

    TiXmlElement *trialSubject = new TiXmlElement("trialSubject");
    trialSubject->SetAttribute("classCode", "RESBJ");
    subject->LinkEndChild(trialSubject);
    
    TiXmlElement *trialSubjectId = new TiXmlElement("id");
    trialSubjectId->SetAttribute("extension", "008");
    trialSubject->LinkEndChild(trialSubjectId);

    TiXmlElement *trialSubjectDemographicPerson = new TiXmlElement("subjectDemographicPerson");
    trialSubjectDemographicPerson->SetAttribute("classCode", "PSN");
    trialSubjectDemographicPerson->SetAttribute("determinerCode", "INSTANCE");
    trialSubject->LinkEndChild(trialSubjectDemographicPerson);

    TiXmlElement *subjectDemographicPersonName = new TiXmlElement("name");
    TiXmlText *nameText = new TiXmlText(hdr->Patient.Name);
    subjectDemographicPersonName->LinkEndChild(nameText);
    trialSubjectDemographicPerson->LinkEndChild(subjectDemographicPersonName);

    TiXmlElement *subjectDemographicPersonGender = new TiXmlElement("administrativeGenderCode");
    if(hdr->Patient.Sex == 1){
	subjectDemographicPersonGender->SetAttribute("code", "M");
	subjectDemographicPersonGender->SetAttribute("displayName", "Male");
    }
    else if(hdr->Patient.Sex == 2){
	subjectDemographicPersonGender->SetAttribute("code", "F");
	subjectDemographicPersonGender->SetAttribute("displayName", "Female");
    }
    else{
	subjectDemographicPersonGender->SetAttribute("code", "UN");
	subjectDemographicPersonGender->SetAttribute("displayName", "Undefined");
    }
    subjectDemographicPersonGender->SetAttribute("codeSystem", "2.16.840.1.113883.5.1");
    subjectDemographicPersonGender->SetAttribute("codeSystemName", "AdministrativeGender");
    trialSubjectDemographicPerson->LinkEndChild(subjectDemographicPersonGender);

    char time[19];
    T0 = gdf_time2t_time(hdr->Patient.Birthday);
    t0 = localtime(&T0);
    sprintf(time, "%4d%2d%2d%2d%2d%2d.000", t0->tm_year+1900, t0->tm_mon+1, t0->tm_mday, t0->tm_hour, t0->tm_min, t0->tm_sec);
    fprintf(stderr, "%4d%2d%2d%2d%2d%2d.000\n\n\n\n", t0->tm_year+1900, t0->tm_mon+1, t0->tm_mday, t0->tm_hour, t0->tm_min, t0->tm_sec);
    for(int i=0; i<18; ++i)
	if(time[i] == ' ')
	    time[i] = '0';

    TiXmlElement *subjectDemographicPersonBirthtime = new TiXmlElement("birthTime");
    subjectDemographicPersonBirthtime->SetAttribute("value", strdup(time));
    trialSubjectDemographicPerson->LinkEndChild(subjectDemographicPersonBirthtime);

    TiXmlElement *subjectAssignmentComponentOf = new TiXmlElement("componentOf");
    subjectAssignmentComponentOf->SetAttribute("typeCode", "COMP");
    subjectAssignmentComponentOf->SetAttribute("contextConductionInd", "true");
    subjectAssignment->LinkEndChild(subjectAssignmentComponentOf);

    TiXmlElement *clinicalTrial = new TiXmlElement("clinicalTrial");
    clinicalTrial->SetAttribute("classCode", "CLNTRL");
    clinicalTrial->SetAttribute("moodCode", "EVN");
    subjectAssignmentComponentOf->LinkEndChild(clinicalTrial);

    TiXmlElement *clinicalTrialId = new TiXmlElement("id");
    clinicalTrialId->SetAttribute("root", "GRATZ");
    clinicalTrialId->SetAttribute("extension", "CLINICAL_TRIAL");
    clinicalTrial->LinkEndChild(clinicalTrialId);
    
    TiXmlElement *rootComponent = new TiXmlElement("component");
    rootComponent->SetAttribute("typeCode", "COMP");
    rootComponent->SetAttribute("contextConductionInd", "true");
    root->LinkEndChild(rootComponent);
    
    TiXmlElement *series = new TiXmlElement("series");
    series->SetAttribute("classCode", "OBSSER");
    series->SetAttribute("moodCode", "EVN");
    rootComponent->LinkEndChild(series);
    
    TiXmlElement *seriesCode = new TiXmlElement("code");
    seriesCode->SetAttribute("code", "RHYTHM");
    seriesCode->SetAttribute("seriesCode", "2.16.840.1.113883.5.4");
    series->LinkEndChild(seriesCode);
    
    TiXmlElement *seriesEffectiveTime = new TiXmlElement("effectiveTime");
    TiXmlElement *seriesEffectiveTimeLow = new TiXmlElement("low");
    seriesEffectiveTimeLow->SetAttribute("value", timelow);
    seriesEffectiveTime->LinkEndChild(seriesEffectiveTimeLow);
    TiXmlElement *seriesEffectiveTimeHigh = new TiXmlElement("high");
    seriesEffectiveTimeHigh->SetAttribute("value", timehigh);
    seriesEffectiveTime->LinkEndChild(seriesEffectiveTimeHigh);
    series->LinkEndChild(seriesEffectiveTime);
    
    for(int i=3; i; --i){
	    TiXmlElement *seriesControlVariable = new TiXmlElement("controlVariable");
	    seriesControlVariable->SetAttribute("typeCode", "CTRLV");
	    series->LinkEndChild(seriesControlVariable);
	    
	    TiXmlElement *CTRLControlVariable = new TiXmlElement("controlVariable");
	    CTRLControlVariable->SetAttribute("classCode", "OBS");
	    seriesControlVariable->LinkEndChild(CTRLControlVariable);
	    
	    TiXmlElement *controlVariableCode = new TiXmlElement("code");
	    CTRLControlVariable->LinkEndChild(controlVariableCode);
    
	    TiXmlElement *controlVariableComponent = new TiXmlElement("component");
	    controlVariableComponent->SetAttribute("typeCode", "COMP");
	    CTRLControlVariable->LinkEndChild(controlVariableComponent);
	    
	    TiXmlElement *componentControlVariable = new TiXmlElement("controlVariable");
	    componentControlVariable->SetAttribute("classCode", "OBS");
	    controlVariableComponent->LinkEndChild(componentControlVariable);
	    
	    TiXmlElement *componentControlVariableCode = new TiXmlElement("code");
	    componentControlVariable->LinkEndChild(componentControlVariableCode);
	    
	    TiXmlElement *componentControlVariableValue = new TiXmlElement("value");
	    componentControlVariableValue->SetAttribute("xsi:type", "PQ");
	    componentControlVariable->LinkEndChild(componentControlVariableValue);
	    
	    switch(i){
		case 3:
		    controlVariableCode->SetAttribute("code", "MDC_ATTR_FILTER_NOTCH");
		    componentControlVariableCode->SetAttribute("code", "MDC_ATTR_NOTCH_FREQ");
		    componentControlVariableValue->SetDoubleAttribute("value", hdr->CHANNEL[0].Notch);
		    break;
		case 2:		    
		    controlVariableCode->SetAttribute("code", "MDC_ATTR_FILTER_LOW_PASS");
		    componentControlVariableCode->SetAttribute("code", "MDC_ATTR_FILTER_CUTOFF_FREQ");
		    componentControlVariableValue->SetDoubleAttribute("value", hdr->CHANNEL[0].LowPass);
		    break;
		case 1:
		    controlVariableCode->SetAttribute("code", "MDC_ATTR_FILTER_HIGH_PASS");
		    componentControlVariableCode->SetAttribute("code", "MDC_ATTR_FILTER_CUTOFF_FREQ");
		    componentControlVariableValue->SetDoubleAttribute("value", hdr->CHANNEL[0].HighPass);
		    break;
	    }
	    
	    controlVariableCode->SetAttribute("codeSystem", "2.16.840.1.113883.6.24");
	    controlVariableCode->SetAttribute("codeSystemName", "MDC");
	    componentControlVariableCode->SetAttribute("codeSystem", "2.16.840.1.113883.6.24");
	    componentControlVariableCode->SetAttribute("codeSystemName", "MDC");
	    componentControlVariableValue->SetAttribute("unit", "Hz");
	    
	    switch(i){
		case 3:
		    controlVariableCode->SetAttribute("displayName", "Notch Filter");
		    componentControlVariableCode->SetAttribute("displayName", "Notch Frequency");
		    break;
		case 2:		    
		    controlVariableCode->SetAttribute("displayName", "Low Pass Filter");
		    componentControlVariableCode->SetAttribute("displayName", "Cutoff Frequency");
		    break;
		case 1:
		    controlVariableCode->SetAttribute("displayName", "High Pass Filter");
		    componentControlVariableCode->SetAttribute("displayName", "Cutoff Frequency");
		    break;
	    }
    }
    
    TiXmlElement *seriesComponent = new TiXmlElement("component");
    seriesComponent->SetAttribute("typeCode", "COMP");
    seriesComponent->SetAttribute("contextConductionInd", "true");
    series->LinkEndChild(seriesComponent);
    
    TiXmlElement *sequenceSet = new TiXmlElement("sequenceSet");
    sequenceSet->SetAttribute("classCode", "OBSCOR");
    sequenceSet->SetAttribute("moodCode", "EVN");
    seriesComponent->LinkEndChild(sequenceSet);
    
    TiXmlElement *sequenceSetComponent = new TiXmlElement("component");
    sequenceSetComponent->SetAttribute("typeCode", "COMP");
    sequenceSetComponent->SetAttribute("contextConductionInd", "true");
    sequenceSet->LinkEndChild(sequenceSetComponent);
    
    TiXmlElement *sequence = new TiXmlElement("sequence");
    sequence->SetAttribute("classCode", "OBS");
    sequence->SetAttribute("moodCode", "EVN");    
    sequenceSetComponent->LinkEndChild(sequence);
    
    TiXmlElement *sequenceCode = new TiXmlElement("code");
    sequenceCode->SetAttribute("code", "TIME_ABSOLUTE");
    sequenceCode->SetAttribute("codeSystem", "2.16.840.1.113883.6.24");
    sequence->LinkEndChild(sequenceCode);

    TiXmlElement *sequenceValue = new TiXmlElement("value");
    sequenceValue->SetAttribute("xsi:type", "GLIST_TS");
    sequence->LinkEndChild(sequenceValue);

    TiXmlElement *valueHead = new TiXmlElement("head");
    valueHead->SetAttribute("value", timelow);
    valueHead->SetAttribute("unit", "s");
    sequenceValue->LinkEndChild(valueHead);

    TiXmlElement *valueIncrement = new TiXmlElement("increment");
    valueIncrement->SetDoubleAttribute("value", 1/hdr->SampleRate);
    valueIncrement->SetAttribute("unit", "s");
    sequenceValue->LinkEndChild(valueIncrement);

    TiXmlText *digitsText;

    for(int i=0; i<hdr->NS; ++i){
	sequenceSetComponent = new TiXmlElement("component");
	sequenceSetComponent->SetAttribute("typeCode", "COMP");
	sequenceSetComponent->SetAttribute("contextConductionInd", "true");
	sequenceSet->LinkEndChild(sequenceSetComponent);

	sequence = new TiXmlElement("sequence");
	sequence->SetAttribute("classCode", "OBS");
	sequence->SetAttribute("moodCode", "EVN");
	sequenceSetComponent->LinkEndChild(sequence);

	sequenceCode = new TiXmlElement("code");
	
	switch(hdr->CHANNEL[i].LeadIdCode){
	    case 1:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_I");
		break;
	    case 2:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_II");
		break;
	    case 61:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_III");
		break;
	    case 62:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_AVR");
		break;
	    case 63:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_AVL");
		break;
	    case 64:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_AVF");
		break;
	    case 3:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_V1");
		break;
	    case 4:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_V2");
		break;
	    case 5:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_V3");
		break;
	    case 6:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_V4");
		break;
	    case 7:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_V5");
		break;
	    case 8:
		sequenceCode->SetAttribute("code", "MDC_ECG_LEAD_V6");
		break;
	}
	sequenceCode->SetAttribute("codeSystem", "2.16.840.1.113883.6.24");
	sequenceCode->SetAttribute("codeSystemName", "MDC");
	sequence->LinkEndChild(sequenceCode);
    
	sequenceValue = new TiXmlElement("value");
	sequenceValue->SetAttribute("xsi:type", "SLIST_PQ");
	sequence->LinkEndChild(sequenceValue);

	valueHead = new TiXmlElement("origin");
	valueHead->SetDoubleAttribute("value", hdr->CHANNEL[i].Off*PhysDimScale(hdr->CHANNEL[i].PhysDimCode)*1e6);
	valueHead->SetAttribute("unit", "uV");
	sequenceValue->LinkEndChild(valueHead);

	valueIncrement = new TiXmlElement("scale");
	valueIncrement->SetDoubleAttribute("value", hdr->CHANNEL[i].Cal*PhysDimScale(hdr->CHANNEL[i].PhysDimCode)*1e6);
	valueIncrement->SetAttribute("unit", "uV");
	sequenceValue->LinkEndChild(valueIncrement);

	TiXmlElement *valueDigits = new TiXmlElement("digits");
	sequenceValue->LinkEndChild(valueDigits);

	std::stringstream digitsStream;
	
	for(unsigned int j=0; j<hdr->CHANNEL[i].SPR; ++j)
	    digitsStream << hdr->data.block[hdr->SPR*i + j] << " ";

	digitsText = new TiXmlText(digitsStream.str().c_str());
	valueDigits->LinkEndChild(digitsText);
    }

    doc.SaveFile(hdr->FileName);

    return(hdr);
};

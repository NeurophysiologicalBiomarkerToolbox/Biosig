#include "cnt_reader.h" 
#include "../stream_utils.h"
#include "../signal_data_block.h"
#include "../math_utils.h"
#include "../gdf/gdf_event.h"

#include <biosig.h>

#include <QTextStream>
#include <QTranslator>
#include <QMutexLocker>

#include <iostream>

namespace BioSig_
{




//-----------------------------------------------------------------------------
CNTReader::CNTReader() :
    basic_header_ (new BasicHeader ()),
    biosig_header_ (0)
{
    // nothing to do here
}

//-----------------------------------------------------------------------------
CNTReader::~CNTReader()
{
    if (biosig_header_)
    {
        sclose(biosig_header_);
        delete biosig_header_;
    }
}

//-----------------------------------------------------------------------------
FileSignalReader* CNTReader::clone()
{
    CNTReader *new_instance = new CNTReader ();
    return new_instance;
}

//-----------------------------------------------------------------------------
void CNTReader::close() 
{
    if (biosig_header_)
    {
        sclose(biosig_header_);
    }
    delete biosig_header_;
    biosig_header_ = 0;
}


//-----------------------------------------------------------------------------
void CNTReader::loadEvents(SignalEventVector& event_vector)
{    
    QMutexLocker lock (&mutex_); 
    if (!biosig_header_)
        return;

    QVector<GDFEvent> gdf_events(biosig_header_->EVENT.N);
    QVector<GDFEvent>::iterator iter;
    
    uint32 event_nr = 0;
    for (iter = gdf_events.begin(); iter != gdf_events.end(); iter++, event_nr++)
    {
        iter->type = biosig_header_->EVENT.TYP[event_nr];
        iter->position = biosig_header_->EVENT.POS[event_nr];
        if (biosig_header_->EVENT.CHN)
        {
            iter->channel = biosig_header_->EVENT.CHN[event_nr];
            iter->duration = biosig_header_->EVENT.DUR[event_nr];
        }
    }
    

    // sort events by position, type and channel
    qSort(gdf_events); // TODO
    //    std::sort(gdf_events.begin(), gdf_events.end(), std::less<GDFEvent>());

    // store to signal events
    for (iter = gdf_events.begin(); iter != gdf_events.end(); iter++)
    {
        if (iter->type & SignalEvent::EVENT_END)
        {
            uint32 start_type = iter->type - SignalEvent::EVENT_END;
            bool start_found = false;
            FileSignalReader::SignalEventVector::iterator rev_iter = event_vector.end();
            while (rev_iter != event_vector.begin())
            {
                rev_iter--;
                if (rev_iter->getType() == start_type &&
                        rev_iter->getDuration() == SignalEvent::UNDEFINED_DURATION)
                {
                    rev_iter->setDuration(iter->position -
                                          rev_iter->getPosition());
                    start_found = true;
                    break;
                }
            }
            if (!start_found)
            {
//                if (log_stream_)
//                {
//                    *log_stream_ << "GDFReader::loadEvents Warning: unexpected "
//                                 << " end-event\n";
//                }
            }
        }
        else
        {
            if (biosig_header_->EVENT.CHN)
            {
                event_vector << SignalEvent(iter->position, iter->type,
                                            (int32)iter->channel - 1,
                                            iter->duration);
            }
            else
            {
                event_vector << SignalEvent(iter->position, iter->type);
            }
        }
    }
}

//-----------------------------------------------------------------------------
bool CNTReader::open(const QString& file_name)
{

    QMutexLocker lock (&mutex_); 
    if (!loadFixedHeader(file_name) /*||
        !loadSignalHeaders(file_name)*/)
    {
        //file_.close();
//        resetBasicHeader();
//        resetGDFHeader();
        return false;
    }
//    loadEventTableHeader();
    
    return true;
}

//-----------------------------------------------------------------------------
void CNTReader::loadSignals(SignalDataBlockPtrIterator begin,
                            SignalDataBlockPtrIterator end, uint32 start_record)
{
    QMutexLocker lock (&mutex_);
    if (!biosig_header_)
    {
//        if (log_stream_) 
//        {
//            *log_stream_ << "GDFReader::loadChannels Error: not open\n";
//        }
        return;
    }
    
//
    bool something_done = true;
    for (uint32 rec_nr = start_record; something_done; rec_nr++)
    {
        bool rec_out_of_range = (rec_nr >= basic_header_->getNumberRecords());
        if (!rec_out_of_range)
        {
            // TODO??? readStreamValues(buffer_, *file_, basic_header_->getRecordSize());
        }
        something_done = false;
        for (FileSignalReader::SignalDataBlockPtrIterator data_block = begin;
             data_block != end;
             data_block++)
        {
//            if ((*data_block)->sub_sampling > 1 ||
//                (*data_block)->channel_number >= basic_header_->getNumberChannels())
//            {
////                if (log_stream_)
////                {
////                    *log_stream_ << "GDFReader::loadChannels Error: "
////                                 << "invalid SignalDataBlock\n";
////                }
//                continue;
//            }
            SignalChannel* sig = basic_header_->getChannelPointer((*data_block)->channel_number);
            uint32 samples = sig->getSamplesPerRecord();
            //std::cout << " samples = " << samples << ";" ;
            uint32 actual_sample = (rec_nr - start_record) * samples; 
            //std::cout << " actual_sample = " << actual_sample << ";" ;
            //std::cout << " (*data_block)->number_samples = " << (*data_block)->number_samples << ";" ;
            
            if (actual_sample + samples > (*data_block)->number_samples)
            {
                if (actual_sample >= (*data_block)->number_samples)
                {
                    (*data_block)->setBufferOffset(start_record * samples);
                    continue;   // signal data block full
                }
                samples = (*data_block)->number_samples - actual_sample;
            }
            float32* data_block_buffer = (*data_block)->getBuffer();
            float32* data_block_upper_buffer = (*data_block)->getUpperBuffer();
            float32* data_block_lower_buffer = (*data_block)->getLowerBuffer();
            bool* data_block_buffer_valid = (*data_block)->getBufferValid();
            something_done = true;
            if (rec_out_of_range)
            {
                for (uint32 samp = actual_sample;
                     samp < actual_sample + samples;
                     samp++)
                {
                    data_block_buffer_valid[samp] = false; 
                }
            }
            else
            {
                static uint32 old_rec_nr = -1;
                static double *read_data = 0;
                if (rec_nr != old_rec_nr)
                {
                    delete read_data;
                    read_data = new double[samples * biosig_header_->NS];
                    memset(read_data, 0, sizeof(read_data));
                    size_t read_length = sread(read_data, rec_nr, samples, biosig_header_);
                    size_t read_samples = biosig_header_->data.size[0];
                    size_t read_channels = biosig_header_->data.size[1];
                    //std::cout << "read_samples = " << read_samples << "; read_channels = "<< read_channels << "; read_length = " << read_length << std::endl;
                    
                }
                old_rec_nr = rec_nr;
//               convertData(blub, &data_block_buffer[actual_sample],
//                            *sig, samples);
                //std::cout << blub[0] << ", ";
                for (uint32 samp = actual_sample;
                     samp < actual_sample + samples;
                     samp++)
                {
                    data_block_buffer[samp] = read_data[(*data_block)->channel_number];
                    data_block_upper_buffer[samp] = data_block_buffer[samp];
                    data_block_lower_buffer[samp] = data_block_buffer[samp];
                    data_block_buffer_valid[samp] = true;
                        //= data_block_buffer[samp] > sig->getPhysicalMinimum() &&
                        //  data_block_buffer[samp] < sig->getPhysicalMaximum();
                }
            }

        }
    }

}

//-----------------------------------------------------------------------------
QPointer<BasicHeader> CNTReader::getBasicHeader ()
{
    QMutexLocker lock (&mutex_);
    return QPointer<BasicHeader>(basic_header_);
}

//-----------------------------------------------------------------------------
bool CNTReader::loadRawRecords(float64** record_data, uint32 start_record,
                               uint32 records)
{

    return false;
} 

//-----------------------------------------------------------------------------
bool CNTReader::loadFixedHeader(const QString& file_name)
{
    QMutexLocker locker (&biosig_access_lock_);
    char *c_file_name = new char[file_name.length()];
    strcpy (c_file_name, file_name.toLocal8Bit ().data());
    
    tzset();
     biosig_header_ = sopen(c_file_name, "r", NULL);
    if (serror() || biosig_header_ == NULL) 
        return false; 
    biosig_header_->FLAG.UCAL = 0;  
    
    
    basic_header_->setFullFileName (file_name);
    basic_header_->setType ("TYPE");
    basic_header_->setNumberChannels(biosig_header_->NS);
    basic_header_->setVersion (QString::number(biosig_header_->VERSION));
    basic_header_->setNumberRecords (biosig_header_->NRec);
    basic_header_->setRecordSize (biosig_header_->CHANNEL[0].SPR); // TODO: different channels different sample rate!!
    basic_header_->setRecordsPosition (biosig_header_->HeadLen); 
    basic_header_->setRecordDuration (static_cast<double>(biosig_header_->Dur[0]) / biosig_header_->Dur[1]);
    basic_header_->setRecordDuration (1.0f / biosig_header_->SampleRate);
    basic_header_->setNumberEvents(biosig_header_->EVENT.N);
    if (biosig_header_->EVENT.SampleRate)
        basic_header_->setEventSamplerate(biosig_header_->EVENT.SampleRate);
    else
        basic_header_->setEventSamplerate(biosig_header_->SampleRate);
    
    for (uint32 channel_index = 0; channel_index < biosig_header_->NS; ++channel_index)
    {
        SignalChannel* channel = new SignalChannel(channel_index, QT_TR_NOOP(biosig_header_->CHANNEL[channel_index].Label), 
                                                   biosig_header_->CHANNEL[channel_index].SPR,
                                                   biosig_header_->CHANNEL[channel_index].PhysDim, 
                                                   biosig_header_->CHANNEL[channel_index].PhysMin,
                                                   biosig_header_->CHANNEL[channel_index].PhysMax,
                                                   biosig_header_->CHANNEL[channel_index].DigMin,
                                                   biosig_header_->CHANNEL[channel_index].DigMax,
                                                   17,
                                                   1 / 8, // TODO: really don't know what that means!
                                                   "filter", -1, -1, false);
        basic_header_->addChannel(channel);   
        //buffer_ = new int8[basic_header_->getRecordSize() * basic_header_->getNumberChannels()];
    }
    
    return true;
}

}

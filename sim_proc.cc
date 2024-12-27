#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "sim_proc.h"
#include <queue>
#include <list>

int universal_clock = 1;
int dynamicInstructionCount = 0;

class ROB_rotations
{
public:
    int ROB_head_index;
    int ROB_tail_index;
    int head_cycle;
    int tail_cycle;

} Rotation;

class InstructionParameters
{
public:
    unsigned long PC;
    int SR1_before_rename, SR2_before_rename;
    int operationtype;
    int destination_register;
    int source_register_1;
    int source_register_2;
    int sequence_number;
    int remaining_exec_cycles;
    bool SR1_ready;
    bool SR2_ready;
    void SetRemainingExecCycles()
    {
        if (operationtype == 0)
            remaining_exec_cycles = 1;
        else if (operationtype == 1)
            remaining_exec_cycles = 2;
        else if (operationtype == 2)
            remaining_exec_cycles = 5;
    }

    int FE_start, FE_duration;
    int DE_start, DE_duration;
    int RN_start, RN_duration;
    int RR_start, RR_duration;
    int DI_start, DI_duration;
    int IS_start, IS_duration;
    int EX_start, EX_duration;
    int WB_start, WB_duration;
    int RT_start, RT_duration;
};

class IssueQueue
{
public:
    bool valid;
    int destination;
    bool source1_ready;
    bool source2_ready;
    int source1;
    int source2;
    unsigned long value;
    int instruction_type;
    int instruction_sequence_number;
    int remaining_exec_cycles;

    int SR1_before_rename;
    int SR2_before_rename;

    int FE_start, FE_duration;
    int DE_start, DE_duration;
    int RN_start, RN_duration;
    int RR_start, RR_duration;
    int DI_start, DI_duration;
    int IS_start, IS_duration;
    int EX_start, EX_duration;
    int WB_start, WB_duration;
    int RT_start, RT_duration;
};

class Rename_Map_Table
{
public:
    int reg;
    bool valid;
    int ROB;
};

class Reorder_buffer
{
public:
    int ROB;
    unsigned long value;
    int destination;
    bool ready;
    int sequence_number;
    bool valid;

    int source_register_1;
    int source_register_2;
    int operationtype;

    int FE_start, FE_duration;
    int DE_start, DE_duration;
    int RN_start, RN_duration;
    int RR_start, RR_duration;
    int DI_start, DI_duration;
    int IS_start, IS_duration;
    int EX_start, EX_duration;
    int WB_start, WB_duration;
    int RT_start, RT_duration;
};

class ExecuteList
{
public:
    bool valid;
    int destination;
    int source_reg1;
    int source_reg2;
    int instruction_age;
    int instruction_type;
    int remaining_cycles;
    int seq;
    int optype;

    int FE_start, FE_duration;
    int DE_start, DE_duration;
    int RN_start, RN_duration;
    int RR_start, RR_duration;
    int DI_start, DI_duration;
    int IS_start, IS_duration;
    int EX_start, EX_duration;
    int WB_start, WB_duration;
    int RT_start, RT_duration;

    int SR1_before_rename;
    int SR2_before_rename;
};

Rename_Map_Table RMT[67];

std::list<int> ROB_commit_indices;

int cycle_count = 0;
int IQ_count = 0;
int width, iq_size, rob_size;
int sequence_number = 0;

std::queue<InstructionParameters> DE_pipeline_register; // length is width
std::queue<InstructionParameters> RN_pipeline_register;
std::queue<InstructionParameters> RR_pipeline_register;
std::queue<InstructionParameters> DI_pipeline_register;
std::vector<IssueQueue> IQ_pipeline_register(5);
std::vector<ExecuteList> execute_list_pipeline_register(5);
std::vector<Reorder_buffer> ROB(200);
std::list<Reorder_buffer> PrintList;

// std::queue <InstructionParameters> execute_list_pipeline_register; // length is width*5
std::queue<ExecuteList> WB_pipeline_register; // length is width*5
// std::queue <InstructionParameters> ROB_queue;  // length is ROB_SIZE
//_____________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________

int IQ_Invalid_entries()
{
    int invalid_entries = 0;
    for (int i = 0; i < iq_size; ++i)
    {
        if (IQ_pipeline_register[i].valid == false)
            invalid_entries++;
    }
    return invalid_entries;
}
int EL_Invalid_entries()
{
    int invalid_entries = 0;
    for (int i = 0; i < width * 5; ++i)
    {
        if (execute_list_pipeline_register[i].valid == false)
            invalid_entries++;
    }
    return invalid_entries;
}

int ROB_Invalid_entries()
{
    int invalid_entries = 0;
    for (int i = 0; i < rob_size; ++i)
    {
        if (ROB[i].valid == false)
            invalid_entries++;
    }
    return invalid_entries;
}

void printInstruction(Reorder_buffer instr)
{
    printf("%d fu{%d} src{%d,%d} dst{%d} "
           "FE{%d,%d} DE{%d,%d} RN{%d,%d} RR{%d,%d} "
           "DI{%d,%d} IS{%d,%d} EX{%d,%d} WB{%d,%d} RT{%d,%d}\n",
           instr.sequence_number, instr.operationtype,
           instr.source_register_1, instr.source_register_2, instr.destination,
           instr.FE_start, instr.FE_duration, instr.DE_start, instr.DE_duration,
           instr.RN_start, instr.RN_duration, instr.RR_start, instr.RR_duration,
           instr.DI_start, instr.DI_duration, instr.IS_start, instr.IS_duration,
           instr.EX_start, instr.EX_duration, instr.WB_start, instr.WB_duration,
           instr.RT_start, instr.RT_duration);
}

bool Advance_Cycle(FILE *FP)
{

    universal_clock++;
    cycle_count++;

    if (((ROB_Invalid_entries() == rob_size) && feof(FP)))
        return false;
    else
        return true;
}

void UpdateRMT(InstructionParameters instr)
{
    if (instr.source_register_1 == -1)
    {
        instr.SR1_ready = true;
    }
    if (instr.source_register_2 == -1)
    {
        instr.SR2_ready = true;
    }
    if (instr.destination_register == -1)
    {

        ROB[Rotation.ROB_tail_index].valid = true;
        ROB[Rotation.ROB_tail_index].destination = -1;

        RN_pipeline_register.front().destination_register = ROB[Rotation.ROB_tail_index].ROB;

        Rotation.ROB_tail_index++;
        //  }
        if (Rotation.ROB_tail_index == rob_size)
        {
            if (Rotation.tail_cycle == 0)
            {
                Rotation.tail_cycle = 1;
            }
            Rotation.ROB_tail_index = 0;
        }
    }

    for (int i = 0; i < 67; i++)
    {

        if (instr.source_register_1 == RMT[i].reg)
        {
            if (RMT[i].valid == false)
            {
                instr.SR1_ready = true;
            }
            else if (RMT[i].valid == true)
            {

                RN_pipeline_register.front().source_register_1 = RMT[i].ROB;
                //  break;
            }
        }
    }

    for (int i = 0; i < 67; i++)
    {
        if (instr.source_register_2 == RMT[i].reg)
        {
            if (RMT[i].valid == false)
            {
                instr.SR2_ready = true;
            }
            else if (RMT[i].valid == true)
            {

                RN_pipeline_register.front().source_register_2 = RMT[i].ROB;
            }
        }
    }
    for (int i = 0; i < 67; i++)
    {

        if (instr.destination_register == RMT[i].reg)
        {
            if (RMT[i].valid == false)
            {

                RMT[i].valid = true;
                RMT[i].ROB = ROB[Rotation.ROB_tail_index].ROB;
                ROB[Rotation.ROB_tail_index].valid = true;
                ROB[Rotation.ROB_tail_index].destination = instr.destination_register;

                RN_pipeline_register.front().destination_register = ROB[Rotation.ROB_tail_index].ROB;

                Rotation.ROB_tail_index++;
                //    }
                if (Rotation.ROB_tail_index == rob_size)
                {
                    if (Rotation.tail_cycle == 0)
                    {
                        Rotation.tail_cycle = 1;
                    }
                    Rotation.ROB_tail_index = 0;
                }
            }
            else if (RMT[i].valid == true)
            {

                RMT[i].valid = true;
                RMT[i].ROB = ROB[Rotation.ROB_tail_index].ROB;
                ROB[Rotation.ROB_tail_index].valid = true;
                ROB[Rotation.ROB_tail_index].destination = instr.destination_register;

                RN_pipeline_register.front().destination_register = ROB[Rotation.ROB_tail_index].ROB;

                Rotation.ROB_tail_index++;

                if (Rotation.ROB_tail_index == rob_size)
                {
                    if (Rotation.tail_cycle == 0)
                    {
                        Rotation.tail_cycle = 1;
                    }
                    Rotation.ROB_tail_index = 0;
                }
            }
        }
    }
    if (RN_pipeline_register.front().source_register_1 >= 1000)
        RN_pipeline_register.front().SR1_ready = false;
    else
        RN_pipeline_register.front().SR1_ready = true;
    if (RN_pipeline_register.front().source_register_2 >= 1000)
        RN_pipeline_register.front().SR2_ready = false;
    else
        RN_pipeline_register.front().SR2_ready = true;
}

void Retire()
{
    for (int i = 0; i < width; ++i)
    {

        if (ROB[Rotation.ROB_head_index].ready && ROB[Rotation.ROB_head_index].valid)
        {

            ROB[Rotation.ROB_head_index].RT_duration = universal_clock - ROB[Rotation.ROB_head_index].RT_start;

            printInstruction(ROB[Rotation.ROB_head_index]);

            ROB[Rotation.ROB_head_index].valid = false;
            ROB[Rotation.ROB_head_index].ready = false;

            ROB[Rotation.ROB_head_index].sequence_number = -10;

            Rotation.ROB_head_index++;
            if (Rotation.ROB_head_index == rob_size)
            {
                Rotation.ROB_head_index = 0;
                Rotation.tail_cycle = 0;
            }
        }
        if (Rotation.ROB_head_index == Rotation.ROB_tail_index && (ROB[Rotation.ROB_head_index].valid == false))
        {
            break;
        }
        else
        {
            // break;
        }
    }
}

void Writeback()
{
    if (WB_pipeline_register.empty())
        return;

    while (!WB_pipeline_register.empty())
    {

        for (int i = 0; i < rob_size; i++)
        {

            if ((ROB[i].ROB == WB_pipeline_register.front().destination) && (ROB[i].valid))
            {

                ROB[i].ready = true;
                // ROB[i].valid = true;

                ROB[i].sequence_number = WB_pipeline_register.front().seq;
                ROB[i].FE_start = WB_pipeline_register.front().FE_start;
                ROB[i].FE_duration = WB_pipeline_register.front().FE_duration;
                ROB[i].DE_start = WB_pipeline_register.front().DE_start;
                ROB[i].DE_duration = WB_pipeline_register.front().DE_duration;
                ROB[i].RN_start = WB_pipeline_register.front().RN_start;
                ROB[i].RN_duration = WB_pipeline_register.front().RN_duration;
                ROB[i].RR_start = WB_pipeline_register.front().RR_start;
                ROB[i].RR_duration = WB_pipeline_register.front().RR_duration;
                ROB[i].DI_start = WB_pipeline_register.front().DI_start;
                ROB[i].DI_duration = WB_pipeline_register.front().DI_duration;
                ROB[i].IS_start = WB_pipeline_register.front().IS_start;
                ROB[i].IS_duration = WB_pipeline_register.front().IS_duration;
                ROB[i].EX_start = WB_pipeline_register.front().EX_start;
                ROB[i].source_register_1 = WB_pipeline_register.front().SR1_before_rename;
                ROB[i].source_register_2 = WB_pipeline_register.front().SR2_before_rename;
                ROB[i].operationtype = WB_pipeline_register.front().instruction_type;

                ROB[i].EX_duration = WB_pipeline_register.front().EX_duration;
                ROB[i].WB_start = WB_pipeline_register.front().WB_start;

                ROB[i].WB_duration = universal_clock - WB_pipeline_register.front().WB_start;
                ROB[i].RT_start = universal_clock;
            }
        }

        WB_pipeline_register.pop();
    }
}

void Execute()
{
    if (EL_Invalid_entries() == width * 5)
        return;

    for (int i = 0; i < width * 5; ++i)
    {

        ExecuteList instr = execute_list_pipeline_register[i];

        if (instr.remaining_cycles == 1 && instr.valid)
        {
            for (int j = 0; j < iq_size; ++j)
            {
                if (IQ_pipeline_register[j].source1 == instr.destination)
                {
                    IQ_pipeline_register[j].source1_ready = true;
                }
                if (IQ_pipeline_register[j].source2 == instr.destination)
                {
                    IQ_pipeline_register[j].source2_ready = true;
                }
            }

            std::queue<InstructionParameters> temp_DI_queue;
            while (!DI_pipeline_register.empty())
            {
                InstructionParameters di_instr = DI_pipeline_register.front();
                DI_pipeline_register.pop();

                if (di_instr.source_register_1 == instr.destination)
                {

                    di_instr.SR1_ready = true;
                }
                if (di_instr.source_register_2 == instr.destination)
                {
                    di_instr.SR2_ready = true;
                }
                temp_DI_queue.push(di_instr);
            }
            DI_pipeline_register = temp_DI_queue;

           
            std::queue<InstructionParameters> temp_RR_queue;
            while (!RR_pipeline_register.empty())
            {
                InstructionParameters rr_instr = RR_pipeline_register.front();
                RR_pipeline_register.pop();

                if (rr_instr.source_register_1 == instr.destination)
                {
                    rr_instr.SR1_ready = true;
                }
                if (rr_instr.source_register_2 == instr.destination)
                {
                    rr_instr.SR2_ready = true;
                }
                temp_RR_queue.push(rr_instr);
            }
            RR_pipeline_register = temp_RR_queue;

       
        }
        if (instr.remaining_cycles == 1 && instr.valid == true)
        {
            instr.EX_duration = universal_clock - instr.EX_start;
            instr.WB_start = universal_clock;

            for (int p = 0; p < 67; p++)
            {
                if (RMT[p].ROB == instr.destination)
                {

                    RMT[p].valid = false;
                    // break;
                }
            }

            WB_pipeline_register.push(instr);

            execute_list_pipeline_register[i].valid = false;
        }

        if (execute_list_pipeline_register[i].valid == true)
        {

            execute_list_pipeline_register[i].remaining_cycles--;
        }
    }
}

void Issue()
{
    if (IQ_Invalid_entries() == iq_size)
        return;
    int width_check = 0;

    while ((EL_Invalid_entries()) && (!(IQ_Invalid_entries() == iq_size)) && (width_check != width))
    {
        int oldest_index = -1;
        int oldest_age = 99999999;

        for (int i = 0; i < iq_size; ++i)
        {
            if (IQ_pipeline_register[i].valid &&
                IQ_pipeline_register[i].source1_ready &&
                IQ_pipeline_register[i].source2_ready)
            {
                if (IQ_pipeline_register[i].instruction_sequence_number < oldest_age)
                {
                    oldest_age = IQ_pipeline_register[i].instruction_sequence_number;
                    oldest_index = i;
                }
            }
        }

        if (oldest_index == -1)
            break;

        ExecuteList temp_execute;
        temp_execute.valid = true;
        temp_execute.destination = IQ_pipeline_register[oldest_index].destination;
        temp_execute.source_reg1 = IQ_pipeline_register[oldest_index].source1;
        temp_execute.source_reg2 = IQ_pipeline_register[oldest_index].source2;
        temp_execute.instruction_age = IQ_pipeline_register[oldest_index].instruction_sequence_number;
        temp_execute.instruction_type = IQ_pipeline_register[oldest_index].instruction_type;
        temp_execute.remaining_cycles = IQ_pipeline_register[oldest_index].remaining_exec_cycles;
        temp_execute.seq = IQ_pipeline_register[oldest_index].instruction_sequence_number;
        temp_execute.optype = IQ_pipeline_register[oldest_index].instruction_type;

        temp_execute.FE_start = IQ_pipeline_register[oldest_index].FE_start;
        temp_execute.FE_duration = IQ_pipeline_register[oldest_index].FE_duration;
        temp_execute.DE_start = IQ_pipeline_register[oldest_index].DE_start;
        temp_execute.DE_duration = IQ_pipeline_register[oldest_index].DE_duration;
        temp_execute.RN_start = IQ_pipeline_register[oldest_index].RN_start;
        temp_execute.RN_duration = IQ_pipeline_register[oldest_index].RN_duration;
        temp_execute.RR_start = IQ_pipeline_register[oldest_index].RR_start;
        temp_execute.RR_duration = IQ_pipeline_register[oldest_index].RR_duration;
        temp_execute.DI_start = IQ_pipeline_register[oldest_index].DI_start;
        temp_execute.DI_duration = IQ_pipeline_register[oldest_index].DI_duration;
        temp_execute.IS_start = IQ_pipeline_register[oldest_index].IS_start;

        temp_execute.SR1_before_rename = IQ_pipeline_register[oldest_index].SR1_before_rename;
        temp_execute.SR2_before_rename = IQ_pipeline_register[oldest_index].SR2_before_rename;

        temp_execute.IS_duration = universal_clock - IQ_pipeline_register[oldest_index].IS_start;
        temp_execute.EX_start = universal_clock;

        for (int i = 0; i < width * 5; ++i)
        {
            if (execute_list_pipeline_register[i].valid == false)
            {

                execute_list_pipeline_register[i] = temp_execute;
                width_check++;
                IQ_pipeline_register[oldest_index].valid = false;

                break;
            }
        }
        if (width_check == width)
        {
            break;
        }
    }
}
void Dispatch()
{
    if (DI_pipeline_register.size() > IQ_Invalid_entries())
        return;
    if (DI_pipeline_register.empty())
        return;

    IssueQueue temp_iq;

    while ((IQ_Invalid_entries() >= DI_pipeline_register.size()) && (!DI_pipeline_register.empty()))
    {
        temp_iq.valid = true;
        temp_iq.destination = DI_pipeline_register.front().destination_register;
        temp_iq.source1 = DI_pipeline_register.front().source_register_1;
        temp_iq.source2 = DI_pipeline_register.front().source_register_2;
       
        for (int i = 0; i < iq_size; ++i)
        {
            if (IQ_pipeline_register[i].valid == false)
            {
                IQ_pipeline_register[i].valid = true;
                IQ_pipeline_register[i].destination = temp_iq.destination;
                IQ_pipeline_register[i].source1 = DI_pipeline_register.front().source_register_1;
                IQ_pipeline_register[i].source2 = DI_pipeline_register.front().source_register_2;
                IQ_pipeline_register[i].source1_ready = DI_pipeline_register.front().SR1_ready;
                IQ_pipeline_register[i].source2_ready = DI_pipeline_register.front().SR2_ready;
                IQ_pipeline_register[i].instruction_type = DI_pipeline_register.front().operationtype;
                IQ_pipeline_register[i].instruction_sequence_number = DI_pipeline_register.front().sequence_number;
                IQ_pipeline_register[i].remaining_exec_cycles = DI_pipeline_register.front().remaining_exec_cycles;

                IQ_pipeline_register[i].SR1_before_rename = DI_pipeline_register.front().SR1_before_rename;
                IQ_pipeline_register[i].SR2_before_rename = DI_pipeline_register.front().SR2_before_rename;

                IQ_pipeline_register[i].FE_start = DI_pipeline_register.front().FE_start;
                IQ_pipeline_register[i].FE_duration = DI_pipeline_register.front().FE_duration;
                IQ_pipeline_register[i].DE_start = DI_pipeline_register.front().DE_start;
                IQ_pipeline_register[i].DE_duration = DI_pipeline_register.front().DE_duration;
                IQ_pipeline_register[i].RN_start = DI_pipeline_register.front().RN_start;
                IQ_pipeline_register[i].RN_duration = DI_pipeline_register.front().RN_duration;
                IQ_pipeline_register[i].RR_start = DI_pipeline_register.front().RR_start;
                IQ_pipeline_register[i].RR_duration = DI_pipeline_register.front().RR_duration;
                IQ_pipeline_register[i].DI_start = DI_pipeline_register.front().DI_start;

                IQ_pipeline_register[i].DI_duration = universal_clock - DI_pipeline_register.front().DI_start;
                IQ_pipeline_register[i].IS_start = universal_clock;

                DI_pipeline_register.pop();
                break;
            }
        }
    }
}
void RegRead()
{
    if (!DI_pipeline_register.empty())
        return;
    if (RR_pipeline_register.empty())
        return;
    while (!(RR_pipeline_register.empty())) 
    {

        RR_pipeline_register.front().RR_duration = universal_clock - RR_pipeline_register.front().RR_start;
        RR_pipeline_register.front().DI_start = universal_clock;
        DI_pipeline_register.push(RR_pipeline_register.front());
        RR_pipeline_register.pop();
    }
}
void Rename()
{
    if (!RR_pipeline_register.empty())
        return;
    if (RN_pipeline_register.empty())
        return;
    if (((Rotation.ROB_tail_index == Rotation.ROB_head_index) && (ROB[Rotation.ROB_head_index].valid == 1)))
        return;
    if (ROB_Invalid_entries() < width)
        return;

    while (!(RN_pipeline_register.empty()))
    {

        UpdateRMT(RN_pipeline_register.front());
        RN_pipeline_register.front().RN_duration = universal_clock - RN_pipeline_register.front().RN_start;
        RN_pipeline_register.front().RR_start = universal_clock;
        RR_pipeline_register.push(RN_pipeline_register.front());
        RN_pipeline_register.pop();
    }
}
void Decode()
{
    if (DE_pipeline_register.empty())
        return;

    if (!RN_pipeline_register.empty())
        return;

    while (!(DE_pipeline_register.empty()))
    {

        DE_pipeline_register.front().DE_duration = universal_clock - DE_pipeline_register.front().DE_start;
        DE_pipeline_register.front().RN_start = universal_clock;

        RN_pipeline_register.push(DE_pipeline_register.front());
        DE_pipeline_register.pop();
    }
}

void Fetch(FILE *FP, std::queue<InstructionParameters> &DE_pipeline_register, int width)
{

    if (!DE_pipeline_register.empty())
        return;

    for (int i = 0; i < width; i++)
    {
        InstructionParameters instr;
        unsigned long PC;
        int operationtype, destination_register, source_register_1, source_register_2;

        int result;
        result = fscanf(FP, "%lx %d %d %d %d", &PC, &operationtype, &destination_register, &source_register_1, &source_register_2);

        if (result == EOF)
            break;

        dynamicInstructionCount++;
        instr.PC = PC;
        instr.operationtype = operationtype;
        instr.destination_register = destination_register;
        instr.source_register_1 = source_register_1;
        instr.source_register_2 = source_register_2;
        instr.SR1_before_rename = source_register_1;
        instr.SR2_before_rename = source_register_2;
        instr.sequence_number = sequence_number;
        sequence_number++;
        instr.SetRemainingExecCycles();

        instr.FE_start = universal_clock - 1;

        instr.FE_duration = 1;
        instr.DE_start = universal_clock;
        instr.SR1_ready = false;
        instr.SR2_ready = false;

        DE_pipeline_register.push(instr);
    }
}

int main(int argc, char *argv[])
{

    FILE *FP;
    char *trace_file;             
    proc_params params;           
    int op_type, dest, src1, src2; 
    uint64_t pc;                  
 
    if (argc != 5)
    {
        printf("Error: Wrong number of inputs:%d\n", argc - 1);
        exit(EXIT_FAILURE);
    }

    params.rob_size = strtoul(argv[1], NULL, 10);
    params.iq_size = strtoul(argv[2], NULL, 10);
    params.width = strtoul(argv[3], NULL, 10);
    trace_file = argv[4];
   
    FP = fopen(trace_file, "r");
    if (FP == NULL)
    {
      
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }
    width = params.width;
    rob_size = params.rob_size;
    iq_size = params.iq_size;

    IQ_pipeline_register.resize(params.iq_size);
    execute_list_pipeline_register.resize(5 * params.width);
    ROB.resize(params.rob_size);

    Rotation.head_cycle = 0;
    Rotation.tail_cycle = 0;
    Rotation.ROB_head_index = 0;
    Rotation.ROB_tail_index = 0;

    for (int i = 0; i < 67; i++)
    {
        RMT[i].reg = i;
        RMT[i].valid = false;
        RMT[i].ROB = 0;
    }

    for (int i = 0; i < params.rob_size; ++i)
    {
        ROB[i].valid = false;
        ROB[i].ROB = 1000 + i;
        ROB[i].ready = 0;
        ROB[i].value = -1;
        ROB[i].destination = -10;
    }
    for (int i = 0; i < iq_size; ++i)
    {
        IQ_pipeline_register[i].valid = false;
        IQ_pipeline_register[i].source1_ready = false;
        IQ_pipeline_register[i].source2_ready = false;
        IQ_pipeline_register[i].source1 = -1;
        IQ_pipeline_register[i].source2 = -1;
        IQ_pipeline_register[i].destination = -1;
        IQ_pipeline_register[i].instruction_type = -1;
        IQ_pipeline_register[i].instruction_sequence_number = 99999999;
    }

    for (int i = 0; i < width * 5; ++i)
    {
        execute_list_pipeline_register[i].valid = false;
    }
    int Test_Count = 0;

    do
    {
        Test_Count++;

        Retire();
        Writeback();
        Execute();
        Issue();
        Dispatch();
        RegRead();
        Rename();
        Decode();
        Fetch(FP, DE_pipeline_register, width);

    } while (Advance_Cycle(FP));

    printf("# === Simulator Command =========\n");
    printf("# ./sim %lu %lu %lu %.*s\n", params.rob_size, params.iq_size, params.width, (int)(strrchr(trace_file, '.') - trace_file), trace_file);
    printf("# === Processor Configuration ===\n");
    printf("# ROB_SIZE = %lu\n", params.rob_size);
    printf("# IQ_SIZE  = %lu\n", params.iq_size);
    printf("# WIDTH    = %lu\n", params.width);
    printf("# === Simulation Results ========\n");
    printf("# Dynamic Instruction Count    = %d\n", dynamicInstructionCount);
    printf("# Cycles                       = %d\n", cycle_count);
    printf("# Instructions Per Cycle (IPC) = %.2f\n", (double)dynamicInstructionCount / cycle_count);
    
    return 0;
}

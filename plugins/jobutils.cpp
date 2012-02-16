#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "MiscUtils.h"

#include "modules/Materials.h"
#include "modules/Items.h"
#include "modules/Gui.h"
#include "modules/Job.h"

#include "df/api.h"
#include "df/world.h"
#include "df/ui.h"
#include "df/ui_build_selector.h"
#include "df/ui_build_item_req.h"
#include "df/build_req_choice_genst.h"
#include "df/build_req_choice_specst.h"
#include "df/building_workshopst.h"
#include "df/building_furnacest.h"
#include "df/job.h"
#include "df/job_item.h"
#include "df/job_list_link.h"
#include "df/item.h"
#include "df/tool_uses.h"
#include "df/general_ref.h"

using std::vector;
using std::string;
using std::endl;
using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::ui_build_selector;
using df::global::ui_workshop_job_cursor;
using df::global::job_next_id;

/* Plugin registration */

static bool job_material_hotkey(Core *c, df::viewscreen *top);

static command_result job_material(Core *c, vector <string> & parameters);
static command_result job_duplicate(Core *c, vector <string> & parameters);
static command_result job_cmd(Core *c, vector <string> & parameters);

DFhackCExport const char * plugin_name ( void )
{
    return "jobutils";
}

DFhackCExport command_result plugin_init (Core *c, std::vector <PluginCommand> &commands)
{
    commands.clear();
    if (!world || !ui)
        return CR_FAILURE;

    commands.push_back(
        PluginCommand(
            "job", "General job query and manipulation.",
            job_cmd, false,
            "  job [query]\n"
            "    Print details of the current job. The job can be\n"
            "    selected in a workshop, or the unit/jobs screen.\n"
            "  job list\n"
            "    Print details of all jobs in the selected workshop.\n"
            "  job item-material <item-idx> <material[:subtoken]>\n"
            "    Replace the exact material id in the job item.\n"
            "  job item-type <item-idx> <type[:subtype]>\n"
            "    Replace the exact item type id in the job item.\n"
        )
    );

    if (ui_workshop_job_cursor || ui_build_selector) {
        commands.push_back(
            PluginCommand(
                "job-material", "Alter the material of the selected job.",
                job_material, job_material_hotkey,
                "  job-material <inorganic-token>\n"
                "Intended to be used as a keybinding:\n"
                "  - In 'q' mode, when a job is highlighted within a workshop\n"
                "    or furnace, changes the material of the job. Only inorganic\n"
                "    materials can be used in this mode.\n"
                "  - In 'b' mode, during selection of building components\n"
                "    positions the cursor over the first available choice\n"
                "    with the matching material.\n"
            )
        );
    }

    if (ui_workshop_job_cursor && job_next_id) {
        commands.push_back(
            PluginCommand(
                "job-duplicate", "Duplicate the selected job in a workshop.",
                job_duplicate, workshop_job_hotkey,
                "  - In 'q' mode, when a job is highlighted within a workshop\n"
                "    or furnace building, instantly duplicates the job.\n"
            )
        );
    }

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( Core * c )
{
    return CR_OK;
}

/* UI state guards */

static bool job_material_hotkey(Core *c, df::viewscreen *top)
{
    return workshop_job_hotkey(c, top) ||
           build_selector_hotkey(c, top);
}

/* job-material implementation */

static command_result job_material_in_job(Core *c, MaterialInfo &new_mat)
{
    df::job *job = getSelectedWorkshopJob(c);
    if (!job)
        return CR_FAILURE;

    if (!new_mat.isValid() || new_mat.type != 0)
    {
        c->con.printerr("New job material isn't inorganic: %s\n",
                        new_mat.toString().c_str());
        return CR_FAILURE;
    }

    MaterialInfo cur_mat(job);

    if (!cur_mat.isValid() || cur_mat.type != 0)
    {
        c->con.printerr("Current job material isn't inorganic: %s\n",
                        cur_mat.toString().c_str());
        return CR_FAILURE;
    }

    df::craft_material_class old_class = cur_mat.getCraftClass();
    if (old_class == craft_material_class::None)
    {
        c->con.printerr("Unexpected current material type: %s\n",
                        cur_mat.toString().c_str());
        return CR_FAILURE;
    }
    if (new_mat.getCraftClass() != old_class)
    {
        c->con.printerr("New material %s does not satisfy requirement: %s\n",
                        new_mat.toString().c_str(), ENUM_KEY_STR(craft_material_class, old_class));
        return CR_FAILURE;
    }

    for (size_t i = 0; i < job->job_items.size(); i++)
    {
        df::job_item *item = job->job_items[i];
        MaterialInfo item_mat(item);

        if (item_mat != cur_mat)
        {
            c->con.printerr("Job item %d has different material: %s\n",
                            i, item_mat.toString().c_str());
            return CR_FAILURE;
        }

        if (!new_mat.matches(*item))
        {
            c->con.printerr("Job item %d requirements not satisfied by %s.\n",
                            i, new_mat.toString().c_str());
            return CR_FAILURE;
        }
    }

    // Apply the substitution
    job->mat_type = new_mat.type;
    job->mat_index = new_mat.index;

    for (size_t i = 0; i < job->job_items.size(); i++)
    {
        df::job_item *item = job->job_items[i];
        item->mat_type = new_mat.type;
        item->mat_index = new_mat.index;
    }

    c->con << "Applied material '" << new_mat.toString()
           << "' to job " << ENUM_KEY_STR(job_type,job->job_type) << endl;
    return CR_OK;
}

static bool build_choice_matches(df::ui_build_item_req *req, df::build_req_choicest *choice,
                                 MaterialInfo &new_mat, bool ignore_select)
{
    if (VIRTUAL_CAST_VAR(gen, df::build_req_choice_genst, choice))
    {
        if (gen->mat_type == new_mat.type &&
            gen->mat_index == new_mat.index &&
            (ignore_select || gen->used_count < gen->candidates.size()))
        {
            return true;
        }
    }
    else if (VIRTUAL_CAST_VAR(spec, df::build_req_choice_specst, choice))
    {
        if (spec->candidate &&
            spec->candidate->getActualMaterial() == new_mat.type &&
            spec->candidate->getActualMaterialIndex() == new_mat.index &&
            (ignore_select || !req->candidate_selected[spec->candidate_id]))
        {
            return true;
        }
    }

    return false;
}

static command_result job_material_in_build(Core *c, MaterialInfo &new_mat)
{
    df::ui_build_selector *sel = ui_build_selector;
    df::ui_build_item_req *req = sel->requirements[ui_build_selector->req_index];

    // Loop through matching choices
    bool matches = build_choice_matches(req, sel->choices[sel->sel_index], new_mat, true);

    size_t size = sel->choices.size();
    int base = (matches ? sel->sel_index + 1 : 0);

    for (size_t i = 0; i < size; i++)
    {
        int idx = (base + i) % size;

        if (build_choice_matches(req, sel->choices[idx], new_mat, false))
        {
            sel->sel_index = idx;
            return CR_OK;
        }
    }

    c->con.printerr("Could not find material in list: %s\n", new_mat.toString().c_str());
    return CR_FAILURE;
}

static command_result job_material(Core * c, vector <string> & parameters)
{
    // HOTKEY COMMAND: CORE ALREADY SUSPENDED

    MaterialInfo new_mat;
    if (parameters.size() == 1)
    {
        if (!new_mat.find(parameters[0])) {
            c->con.printerr("Could not find material: %s\n", parameters[0].c_str());
            return CR_WRONG_USAGE;
        }
    }
    else
        return CR_WRONG_USAGE;

    if (ui->main.mode == ui_sidebar_mode::QueryBuilding)
        return job_material_in_job(c, new_mat);
    if (ui->main.mode == ui_sidebar_mode::Build)
        return job_material_in_build(c, new_mat);

    return CR_WRONG_USAGE;
}

/* job-duplicate implementation */

static command_result job_duplicate(Core * c, vector <string> & parameters)
{
    if (!parameters.empty())
        return CR_WRONG_USAGE;

    df::job *job = getSelectedWorkshopJob(c);
    if (!job)
        return CR_FAILURE;

    if (!job->misc_links.empty() ||
        (job->job_items.empty() &&
         job->job_type != job_type::CollectSand &&
         job->job_type != job_type::CollectClay))
    {
        c->con.printerr("Cannot duplicate job %s\n", ENUM_KEY_STR(job_type,job->job_type));
        return CR_FAILURE;
    }

    df::building *building = world->selected_building;
    if (building->jobs.size() >= 10)
    {
        c->con.printerr("Job list is already full.\n");
        return CR_FAILURE;
    }

    // Actually clone
    df::job *pnew = cloneJobStruct(job);

    linkJobIntoWorld(pnew);
    vector_insert_at(building->jobs, ++*ui_workshop_job_cursor, pnew);

    return CR_OK;
}

/* Main job command implementation */

static df::job_item *getJobItem(Core *c, df::job *job, std::string idx)
{
    if (!job)
        return NULL;

    int v = atoi(idx.c_str());
    if (v < 1 || v > job->job_items.size()) {
        c->con.printerr("Invalid item index.\n");
        return NULL;
    }

    return job->job_items[v-1];
}

static command_result job_cmd(Core * c, vector <string> & parameters)
{
    CoreSuspender suspend(c);

    std::string cmd = (parameters.empty() ? "query" : parameters[0]);
    if (cmd == "query" || cmd == "list")
    {
        df::job *job = getSelectedJob(c);
        if (!job)
            return CR_WRONG_USAGE;

        if (cmd == "query") {
            printJobDetails(c, job);
        } else {
            if (!workshop_job_hotkey(c, c->getTopViewscreen()))
                return CR_WRONG_USAGE;

            df::building *selected = world->selected_building;
            for (size_t i = 0; i < selected->jobs.size(); i++)
                printJobDetails(c, selected->jobs[i]);
        }
    }
    else if (cmd == "item-material")
    {
        if (parameters.size() != 3)
            return CR_WRONG_USAGE;

        df::job *job = getSelectedJob(c);
        df::job_item *item = getJobItem(c, job, parameters[1]);
        if (!item)
            return CR_WRONG_USAGE;

        ItemTypeInfo iinfo(item);
        MaterialInfo minfo;

        if (!minfo.find(parameters[2])) {
            c->con.printerr("Could not find the specified material.\n");
            return CR_FAILURE;
        }

        if (minfo.isValid() && !iinfo.matches(*item, &minfo)) {
            c->con.printerr("Material does not match the requirements.\n");
            printJobDetails(c, job);
            return CR_FAILURE;
        }

        if (job->mat_type != -1 &&
            job->mat_type == item->mat_type &&
            job->mat_index == item->mat_index)
        {
            job->mat_type = minfo.type;
            job->mat_index = minfo.index;
        }

        item->mat_type = minfo.type;
        item->mat_index = minfo.index;

        c->con << "Job item updated." << endl;

        if (item->item_type < 0 && minfo.isValid())
            c->con.printerr("WARNING: Due to a probable bug, creature & plant material subtype\n"
                            "         is ignored unless the item type is also specified.\n");

        printJobDetails(c, job);
        return CR_OK;
    }
    else if (cmd == "item-type")
    {
        if (parameters.size() != 3)
            return CR_WRONG_USAGE;

        df::job *job = getSelectedJob(c);
        df::job_item *item = getJobItem(c, job, parameters[1]);
        if (!item)
            return CR_WRONG_USAGE;

        ItemTypeInfo iinfo;
        MaterialInfo minfo(item);

        if (!iinfo.find(parameters[2])) {
            c->con.printerr("Could not find the specified item type.\n");
            return CR_FAILURE;
        }

        if (iinfo.isValid() && !iinfo.matches(*item, &minfo)) {
            c->con.printerr("Item type does not match the requirements.\n");
            printJobDetails(c, job);
            return CR_FAILURE;
        }

        item->item_type = iinfo.type;
        item->item_subtype = iinfo.subtype;

        c->con << "Job item updated." << endl;
        printJobDetails(c, job);
        return CR_OK;
    }
    else
        return CR_WRONG_USAGE;

    return CR_OK;
}

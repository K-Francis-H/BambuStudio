#include "CaliHistoryDialog.hpp"
#include "I18N.hpp"

#include "libslic3r/Utils.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "format.hpp"
#include "MsgDialog.hpp"
#include "slic3r/Utils/CalibUtils.hpp"

namespace Slic3r {
namespace GUI {

  
#define HISTORY_WINDOW_SIZE                wxSize(FromDIP(700), FromDIP(600))
#define EDIT_HISTORY_DIALOG_INPUT_SIZE     wxSize(FromDIP(160), FromDIP(24))
#define HISTORY_WINDOW_ITEMS_COUNT         5

static wxString get_preset_name_by_filament_id(std::string filament_id)
{
    auto preset_bundle = wxGetApp().preset_bundle;
    wxString preset_name = "";
    for (auto it = preset_bundle->filaments.begin(); it != preset_bundle->filaments.end(); it++) {
        if (filament_id.compare(it->filament_id) == 0) {
            if (!it->alias.empty())
                preset_name = it->alias;
            else
                preset_name = it->name;
        }
    }
    return preset_name;
}

HistoryWindow::HistoryWindow(wxWindow* parent, const std::vector<PACalibResult>& calib_results_history)
    : DPIDialog(parent, wxID_ANY, _L("Flow Dynamics Calibration Result"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
    , m_calib_results_history(calib_results_history)
{
    this->SetBackgroundColour(*wxWHITE);
    auto main_sizer = new wxBoxSizer(wxVERTICAL);

    auto scroll_window = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL);
    scroll_window->SetScrollRate(5, 5);
    scroll_window->SetBackgroundColour(*wxWHITE);

    auto scroll_sizer = new wxBoxSizer(wxVERTICAL);
    scroll_window->SetSizer(scroll_sizer);

    wxPanel* comboBox_panel = new wxPanel(scroll_window);
    comboBox_panel->SetBackgroundColour(wxColour(238, 238, 238));
    auto comboBox_sizer = new wxBoxSizer(wxVERTICAL);
    comboBox_panel->SetSizer(comboBox_sizer);
    comboBox_sizer->AddSpacer(10);

    auto nozzle_dia_title = new wxStaticText(comboBox_panel, wxID_ANY, _L("Nozzle Diameter"));
    nozzle_dia_title->SetFont(Label::Head_14);
    comboBox_sizer->Add(nozzle_dia_title, 0, wxLEFT | wxRIGHT, FromDIP(15));
    comboBox_sizer->AddSpacer(10);

    m_comboBox_nozzle_dia = new ComboBox(comboBox_panel, wxID_ANY, "", wxDefaultPosition, wxSize(FromDIP(600), FromDIP(24)), 0, nullptr, wxCB_READONLY);
    comboBox_sizer->Add(m_comboBox_nozzle_dia, 0, wxLEFT | wxRIGHT, FromDIP(15));
    comboBox_sizer->AddSpacer(10);

    scroll_sizer->Add(comboBox_panel);

    scroll_sizer->AddSpacer(FromDIP(15));

    wxPanel* tips_panel = new wxPanel(scroll_window, wxID_ANY);
    auto tips_sizer = new wxBoxSizer(wxVERTICAL);
    tips_panel->SetSizer(tips_sizer);
    m_tips = new wxStaticText(tips_panel, wxID_ANY, "");
    m_tips->SetForegroundColour({ 145, 145, 145 });
    tips_sizer->Add(m_tips, 0, wxEXPAND);

    scroll_sizer->Add(tips_panel, 0, wxEXPAND);

    scroll_sizer->AddSpacer(FromDIP(15));

    m_history_data_panel = new wxPanel(scroll_window);

    scroll_sizer->Add(m_history_data_panel, 1, wxEXPAND);

    main_sizer->Add(scroll_window, 1, wxEXPAND | wxALL, FromDIP(10));
    SetSizer(main_sizer);
    main_sizer->Fit(this);
    SetMinSize(HISTORY_WINDOW_SIZE);
    SetSize(HISTORY_WINDOW_SIZE);
    CenterOnParent();

    wxGetApp().UpdateDlgDarkUI(this);

    m_comboBox_nozzle_dia->Bind(wxEVT_COMBOBOX, &HistoryWindow::on_select_nozzle, this);

    m_refresh_timer = new wxTimer();
    m_refresh_timer->SetOwner(this);
    m_refresh_timer->Start(200);
    Bind(wxEVT_TIMER, &HistoryWindow::on_timer, this);
}

HistoryWindow::~HistoryWindow()
{
    m_refresh_timer->Stop();
}

void HistoryWindow::sync_history_result(MachineObject* obj)
{
    BOOST_LOG_TRIVIAL(info) << "sync_history_result";

    m_calib_results_history.clear();
    if (obj)
        m_calib_results_history = obj->pa_calib_tab;

    sync_history_data();

    if (m_calib_results_history.empty()) {
        m_tips->SetLabel(_L("No History Result"));
    }
    else {
        m_tips->SetLabel("Success to get history result");
    }
    m_tips->Refresh();

}

void HistoryWindow::on_device_connected(MachineObject* obj)
{
    if (!obj) {
        return;
    }

    curr_obj = obj;
    // init nozzle value
    static std::array<float, 4> nozzle_diameter_list = { 0.2f, 0.4f, 0.6f, 0.8f };
    int selection = 1;
    for (int i = 0; i < nozzle_diameter_list.size(); i++) {
        m_comboBox_nozzle_dia->AppendString(wxString::Format("%1.1f mm", nozzle_diameter_list[i]));
        if (abs(curr_obj->nozzle_diameter - nozzle_diameter_list[i]) < 1e-3) {
            selection = i;
        }
    }
    m_comboBox_nozzle_dia->SetSelection(selection);

    // trigger on_select nozzle
    wxCommandEvent evt(wxEVT_COMBOBOX);
    evt.SetEventObject(m_comboBox_nozzle_dia);
    wxPostEvent(m_comboBox_nozzle_dia, evt);
}

void HistoryWindow::on_timer(wxTimerEvent& event)
{
    update(curr_obj);
}

void HistoryWindow::update(MachineObject* obj)
{
    if (!obj) return;

    if (obj->cali_version != history_version) {
        if (obj->has_get_pa_calib_tab) {
            history_version = obj->cali_version;
            reqeust_history_result(obj);
        }
    }

    // sync when history is not empty
    if (obj->has_get_pa_calib_tab && m_calib_results_history.empty()) {
        sync_history_result(curr_obj);
    }
}

void HistoryWindow::on_select_nozzle(wxCommandEvent& evt)
{
    reqeust_history_result(curr_obj);
    
}

void HistoryWindow::reqeust_history_result(MachineObject* obj)
{
    if (curr_obj) {
        curr_obj->reset_pa_cali_history_result();
        sync_history_result(curr_obj);

        float nozzle_value = get_nozzle_value();
        if (nozzle_value > 0) {
            CalibUtils::emit_get_PA_calib_infos(nozzle_value);
            m_tips->SetLabel("Refreshing the historical Flow Dynamics Calibration records");
            BOOST_LOG_TRIVIAL(info) << "request calib history";
        }
    }
}

void HistoryWindow::sync_history_data() {
    m_history_data_panel->DestroyChildren();
    wxGridBagSizer* gbSizer;
    gbSizer = new wxGridBagSizer(FromDIP(0), FromDIP(50));
    gbSizer->SetFlexibleDirection(wxBOTH);
    gbSizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

    m_history_data_panel->SetSizer(gbSizer, true);

    auto title_name = new wxStaticText(m_history_data_panel, wxID_ANY, _L("Name"));
    title_name->SetFont(Label::Head_14);
    gbSizer->Add(title_name, { 0, 0 }, { 1, 1 }, wxBOTTOM, FromDIP(15));

    auto title_preset_name = new wxStaticText(m_history_data_panel, wxID_ANY, _L("Filament Preset"));
    title_preset_name->SetFont(Label::Head_14);
    gbSizer->Add(title_preset_name, { 0, 1 }, { 1, 1 }, wxBOTTOM, FromDIP(15));

    auto title_k = new wxStaticText(m_history_data_panel, wxID_ANY, _L("Factor K"));
    title_k->SetFont(Label::Head_14);
    gbSizer->Add(title_k, { 0, 2 }, { 1, 1 }, wxBOTTOM, FromDIP(15));

    // Hide
    //auto title_n = new wxStaticText(m_history_data_panel, wxID_ANY, _L("N"));
    //title_n->SetFont(Label::Head_14);
    //gbSizer->Add(title_n, { 0, 3 }, { 1, 1 }, wxBOTTOM, FromDIP(15));

    auto title_action = new wxStaticText(m_history_data_panel, wxID_ANY, _L("Action"));
    title_action->SetFont(Label::Head_14);
    gbSizer->Add(title_action, { 0, 3 }, { 1, 1 });

    int i = 1;
    for (auto& result : m_calib_results_history) {
        auto name_value = new wxStaticText(m_history_data_panel, wxID_ANY, result.name);

        wxString preset_name = get_preset_name_by_filament_id(result.filament_id);
        auto preset_name_value = new wxStaticText(m_history_data_panel, wxID_ANY, preset_name);

        auto k_str = wxString::Format("%.3f", result.k_value);
        auto n_str = wxString::Format("%.3f", result.n_coef);
        auto k_value = new wxStaticText(m_history_data_panel, wxID_ANY, k_str);
        auto n_value = new wxStaticText(m_history_data_panel, wxID_ANY, n_str);
        n_value->Hide();
        auto delete_button = new Button(m_history_data_panel, _L("Delete"));
        delete_button->SetBackgroundColour(*wxWHITE);
        delete_button->SetMinSize(wxSize(-1, FromDIP(24)));
        delete_button->SetCornerRadius(FromDIP(12));
        delete_button->Bind(wxEVT_BUTTON, [this, gbSizer, i, &result](auto& e) {
            for (int j = 0; j < HISTORY_WINDOW_ITEMS_COUNT; j++) {
                auto item = gbSizer->FindItemAtPosition({ i, j });
                item->GetWindow()->Hide();
            }
            gbSizer->SetEmptyCellSize({ 0,0 });
            m_history_data_panel->Layout();
            m_history_data_panel->Fit();
            CalibUtils::delete_PA_calib_result({ result.tray_id, result.cali_idx, result.nozzle_diameter, result.filament_id });
            });

        auto edit_button = new Button(m_history_data_panel, _L("Edit"));
        StateColor btn_bg_green(std::pair<wxColour, int>(wxColour(27, 136, 68), StateColor::Pressed),
            std::pair<wxColour, int>(wxColour(61, 203, 115), StateColor::Hovered),
            std::pair<wxColour, int>(wxColour(0, 174, 66), StateColor::Normal));
        edit_button->SetBackgroundColour(*wxWHITE);
        edit_button->SetBackgroundColor(btn_bg_green);
        edit_button->SetBorderColor(wxColour(0, 174, 66));
        edit_button->SetTextColor(wxColour("#FFFFFE"));
        edit_button->SetMinSize(wxSize(-1, FromDIP(24)));
        edit_button->SetCornerRadius(FromDIP(12));
        edit_button->Bind(wxEVT_BUTTON, [this, &result, k_value, name_value](auto& e) {
            PACalibResult result_buffer = result;
            result_buffer.k_value = stof(k_value->GetLabel().ToStdString());
            result_buffer.name = name_value->GetLabel().ToStdString();
            EditCalibrationHistoryDialog dlg(this, result_buffer);
            if (dlg.ShowModal() == wxID_OK) {
                auto new_result = dlg.get_result();

                wxString new_k_str = wxString::Format("%.3f", new_result.k_value);
                k_value->SetLabel(new_k_str);
                name_value->SetLabel(new_result.name);

                new_result.tray_id = -1;
                CalibUtils::set_PA_calib_result({ new_result });
            }
            });

        gbSizer->Add(name_value, { i, 0 }, { 1, 1 }, wxBOTTOM, FromDIP(15));
        gbSizer->Add(preset_name_value, { i, 1 }, { 1, 1 }, wxBOTTOM, FromDIP(15));
        gbSizer->Add(k_value, { i, 2 }, { 1, 1 }, wxBOTTOM, FromDIP(15));
        //gbSizer->Add(n_value, { i, 3 }, { 1, 1 }, wxBOTTOM, FromDIP(15));
        gbSizer->Add(delete_button, { i, 3 }, { 1, 1 }, wxBOTTOM, FromDIP(15));
        gbSizer->Add(edit_button, { i, 4 }, { 1, 1 }, wxBOTTOM, FromDIP(15));
        i++;
    }

    Layout();
}

float HistoryWindow::get_nozzle_value()
{
    double nozzle_value = 0.0;
    wxString nozzle_value_str = m_comboBox_nozzle_dia->GetValue();
    try {
        nozzle_value_str.ToDouble(&nozzle_value);
    }
    catch (...) {
        ;
    }

    return nozzle_value;
}


EditCalibrationHistoryDialog::EditCalibrationHistoryDialog(wxWindow* parent, const PACalibResult& result)
    : DPIDialog(parent, wxID_ANY, _L("Edit Flow Dynamics Calibration"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
    , m_new_result(result)
{
    this->SetBackgroundColour(*wxWHITE);
    auto main_sizer = new wxBoxSizer(wxVERTICAL);

    auto top_panel = new wxPanel(this);
    auto panel_sizer = new wxBoxSizer(wxVERTICAL);
    top_panel->SetSizer(panel_sizer);

    auto flex_sizer = new wxFlexGridSizer(0, 2, FromDIP(15), FromDIP(30));
    flex_sizer->SetFlexibleDirection(wxBOTH);
    flex_sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);

    wxStaticText* name_title = new wxStaticText(top_panel, wxID_ANY, _L("Name"));
    TextInput* name_value = new TextInput(top_panel, m_new_result.name, "", "", wxDefaultPosition, EDIT_HISTORY_DIALOG_INPUT_SIZE, wxTE_PROCESS_ENTER);
    name_value->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this, name_value](auto& e) {
        if (!name_value->GetTextCtrl()->GetValue().IsEmpty())
            m_new_result.name = name_value->GetTextCtrl()->GetValue().ToStdString();
        });
    name_value->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this, name_value](auto& e) {
        if (!name_value->GetTextCtrl()->GetValue().IsEmpty())
            m_new_result.name = name_value->GetTextCtrl()->GetValue().ToStdString();
        e.Skip();
        });
    flex_sizer->Add(name_title);
    flex_sizer->Add(name_value);

    wxStaticText* preset_name_title = new wxStaticText(top_panel, wxID_ANY, _L("Filament Preset"));
    wxString preset_name = get_preset_name_by_filament_id(result.filament_id);
    wxStaticText* preset_name_value = new wxStaticText(top_panel, wxID_ANY, preset_name);
    flex_sizer->Add(preset_name_title);
    flex_sizer->Add(preset_name_value);

    wxStaticText* k_title = new wxStaticText(top_panel, wxID_ANY, _L("Factor K"));
    auto k_str = wxString::Format("%.3f", m_new_result.k_value);
    TextInput* k_value = new TextInput(top_panel, k_str, "", "", wxDefaultPosition, EDIT_HISTORY_DIALOG_INPUT_SIZE, wxTE_PROCESS_ENTER);
    k_value->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this, k_value](auto& e) {
        float k = 0.0f;
        if (!CalibUtils::validate_input_k_value(k_value->GetTextCtrl()->GetValue(), &k)) {
            MessageDialog msg_dlg(nullptr, _L("Please input a valid value (K in 0~0.5)"), wxEmptyString, wxICON_WARNING | wxOK);
            msg_dlg.ShowModal();
        }
        wxString k_str = wxString::Format("%.3f", k);
        k_value->GetTextCtrl()->SetValue(k_str);
        m_new_result.k_value = k;
        });
    k_value->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this, k_value](auto& e) {
        float k = 0.0f;
        if (!CalibUtils::validate_input_k_value(k_value->GetTextCtrl()->GetValue(), &k)) {
            MessageDialog msg_dlg(nullptr, _L("Please input a valid value (K in 0~0.5)"), wxEmptyString, wxICON_WARNING | wxOK);
            msg_dlg.ShowModal();
        }
        wxString k_str = wxString::Format("%.3f", k);
        k_value->GetTextCtrl()->SetValue(k_str);
        m_new_result.k_value = k;
        e.Skip();
        });
    flex_sizer->Add(k_title);
    flex_sizer->Add(k_value);

    // Hide:
    //wxStaticText* n_title = new wxStaticText(top_panel, wxID_ANY, _L("Factor N"));
    //TextInput* n_value = new TextInput(top_panel, n, "", "", wxDefaultPosition, EDIT_HISTORY_DIALOG_INPUT_SIZE, wxTE_PROCESS_ENTER);
    //flex_sizer->Add(n_title);
    //flex_sizer->Add(n_value);

    panel_sizer->Add(flex_sizer);

    panel_sizer->AddSpacer(FromDIP(25));

    auto btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    Button* save_btn = new Button(top_panel, _L("Save"));
    StateColor btn_bg_green(std::pair<wxColour, int>(wxColour(27, 136, 68), StateColor::Pressed),
        std::pair<wxColour, int>(wxColour(61, 203, 115), StateColor::Hovered),
        std::pair<wxColour, int>(wxColour(0, 174, 66), StateColor::Normal));
    save_btn->SetBackgroundColour(*wxWHITE);
    save_btn->SetBackgroundColor(btn_bg_green);
    save_btn->SetBorderColor(wxColour(0, 174, 66));
    save_btn->SetTextColor(wxColour("#FFFFFE"));
    save_btn->SetMinSize(wxSize(-1, FromDIP(24)));
    save_btn->SetCornerRadius(FromDIP(12));
    Button* cancel_btn = new Button(top_panel, _L("Cancel"));
    cancel_btn->SetBackgroundColour(*wxWHITE);
    cancel_btn->SetMinSize(wxSize(-1, FromDIP(24)));
    cancel_btn->SetCornerRadius(FromDIP(12));
    save_btn->Bind(wxEVT_BUTTON, &EditCalibrationHistoryDialog::on_save, this);
    cancel_btn->Bind(wxEVT_BUTTON, &EditCalibrationHistoryDialog::on_cancel, this);
    btn_sizer->AddStretchSpacer();
    btn_sizer->Add(save_btn);
    btn_sizer->AddSpacer(FromDIP(20));
    btn_sizer->Add(cancel_btn);
    panel_sizer->Add(btn_sizer, 0, wxEXPAND, 0);


    main_sizer->Add(top_panel, 1, wxEXPAND | wxALL, FromDIP(20));

    SetSizer(main_sizer);
    Layout();
    Fit();
    CenterOnParent();

    wxGetApp().UpdateDlgDarkUI(this);
}

EditCalibrationHistoryDialog::~EditCalibrationHistoryDialog() {
}

PACalibResult EditCalibrationHistoryDialog::get_result() {
    return m_new_result;
}

void EditCalibrationHistoryDialog::on_save(wxCommandEvent& event) {
    EndModal(wxID_OK);
}

void EditCalibrationHistoryDialog::on_cancel(wxCommandEvent& event) {
    EndModal(wxID_CANCEL);
}

void EditCalibrationHistoryDialog::on_dpi_changed(const wxRect& suggested_rect)
{
}

} // namespace GUI
} // namespace Slic3r

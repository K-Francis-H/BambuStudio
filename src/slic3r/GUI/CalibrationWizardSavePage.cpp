#include "CalibrationWizardSavePage.hpp"
#include "I18N.hpp"
#include "Widgets/Label.hpp"
#include "MsgDialog.hpp"


namespace Slic3r { namespace GUI {


static wxString get_default_name(wxString filament_name, CalibMode mode){
    PresetBundle* preset_bundle = wxGetApp().preset_bundle;
    for (auto it = preset_bundle->filaments.begin(); it != preset_bundle->filaments.end(); it++) {
        if (filament_name.compare(it->name) == 0) {
            if (!it->alias.empty())
                filament_name = it->alias;
            else
                filament_name = it->name;
        }
    }

    switch (mode)
    {
    case Slic3r::CalibMode::Calib_None:
        break;
    case Slic3r::CalibMode::Calib_PA_Line:
        if (filament_name.StartsWith("Generic")) {
            filament_name.Replace("Generic", "Brand", false);
        }
        break;
    case Slic3r::CalibMode::Calib_PA_Tower:
        break;
    case Slic3r::CalibMode::Calib_Flow_Rate:
        filament_name += " Flow Rate Calibrated";
        break;
    case Slic3r::CalibMode::Calib_Temp_Tower:
        filament_name += " Temperature Calibrated";
        break;
    case Slic3r::CalibMode::Calib_Vol_speed_Tower:
        filament_name += " Max Vol Speed Calibrated";
        break;
    case Slic3r::CalibMode::Calib_VFA_Tower:
        break;
    case Slic3r::CalibMode::Calib_Retraction_tower:
        break;
    default:
        break;
    }
    return filament_name;
}

CalibrationCommonSavePage::CalibrationCommonSavePage(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : CalibrationWizardPage(parent, id, pos, size, style)
{
    ;
}

enum class GridTextInputType {
    K,
    N,
    FlowRatio,
    Name
};

class GridTextInput : public TextInput
{
public:
    GridTextInput(wxWindow* parent, wxString text, wxString label, wxSize size, int col_idx, GridTextInputType type);
    int get_col_idx() { return m_col_idx; }
    void set_col_idx(int idx) { m_col_idx = idx; }
    GridTextInputType get_type() { return m_type; }
    void set_type(GridTextInputType type) { m_type = type; }
private:
    int m_col_idx;
    GridTextInputType m_type;
};

GridTextInput::GridTextInput(wxWindow* parent, wxString text, wxString label, wxSize size, int col_idx, GridTextInputType type)
    : TextInput(parent, text, label, "", wxDefaultPosition, size, wxTE_PROCESS_ENTER)
    , m_col_idx(col_idx)
    , m_type(type)
{
}

class GridComboBox : public ComboBox {
public:
    GridComboBox(wxWindow* parent, wxSize size, int col_idx);
    int get_col_idx() { return m_col_idx; }
    void set_col_idx(int idx) { m_col_idx = idx; }
private:
    int m_col_idx;
};

GridComboBox::GridComboBox(wxWindow* parent, wxSize size, int col_idx)
    : ComboBox(parent, wxID_ANY, "", wxDefaultPosition, size, 0, nullptr)
    , m_col_idx(col_idx)
{

}

CaliPASaveAutoPanel::CaliPASaveAutoPanel(
    wxWindow* parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : wxPanel(parent, id, pos, size, style) 
{
    m_top_sizer = new wxBoxSizer(wxVERTICAL);
    
    create_panel(this);
    
    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}


void CaliPASaveAutoPanel::create_panel(wxWindow* parent)
{
    m_complete_text_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_complete_text_panel->Hide();
    wxBoxSizer* complete_text_sizer = new wxBoxSizer(wxVERTICAL);
    auto complete_text = new wxStaticText(m_complete_text_panel, wxID_ANY, _L("We found the best Flow Dynamics Calibration Factor"));
    complete_text->SetFont(Label::Head_14);
    complete_text->Wrap(CALIBRATION_TEXT_MAX_LENGTH);
    complete_text_sizer->Add(complete_text, 0, wxALIGN_CENTER);
    m_complete_text_panel->SetSizer(complete_text_sizer);

    m_part_failed_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_part_failed_panel->SetBackgroundColour(wxColour(238, 238, 238));
    wxBoxSizer* part_failed_sizer = new wxBoxSizer(wxVERTICAL);
    m_part_failed_panel->SetSizer(part_failed_sizer);
    part_failed_sizer->AddSpacer(FromDIP(10));
    auto part_failed_text = new wxStaticText(m_part_failed_panel, wxID_ANY, _L("Part of the calibration failed! You may clean the plate and retry. The failed test result would be droped."));
    part_failed_text->SetFont(Label::Body_14);
    part_failed_text->Wrap(CALIBRATION_TEXT_MAX_LENGTH);
    part_failed_sizer->Add(part_failed_text, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(20));
    part_failed_sizer->AddSpacer(FromDIP(10));

    m_top_sizer->Add(m_complete_text_panel, 0, wxALIGN_CENTER, 0);

    m_top_sizer->Add(m_part_failed_panel, 0, wxALIGN_CENTER, 0);

    m_top_sizer->AddSpacer(FromDIP(20));

    m_grid_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_top_sizer->Add(m_grid_panel, 0, wxALIGN_CENTER);

    m_top_sizer->AddSpacer(PRESET_GAP);

    wxStaticText* naming_hints = new wxStaticText(parent, wxID_ANY, _L("*We recommend you to add brand, materia, type, and even humidity level in the Name"));
    naming_hints->SetFont(Label::Body_14);
    naming_hints->SetForegroundColour(wxColour(157, 157, 157));
    m_top_sizer->Add(naming_hints, 0, wxALIGN_CENTER, 0);

    m_top_sizer->AddSpacer(FromDIP(20));
}

void CaliPASaveAutoPanel::sync_cali_result(const std::vector<PACalibResult>& cali_result, const std::vector<PACalibResult>& history_result)
{
    m_history_results = history_result;
    m_calib_results.clear();
    for (auto& item : cali_result) {
        if (item.confidence == 0)
            m_calib_results[item.tray_id] = item;
    }
    m_grid_panel->DestroyChildren();
    auto grid_sizer = new wxBoxSizer(wxHORIZONTAL);
    const int COLUMN_GAP = FromDIP(50);
    const int ROW_GAP = FromDIP(30);
    wxBoxSizer* left_title_sizer = new wxBoxSizer(wxVERTICAL);
    left_title_sizer->AddSpacer(FromDIP(52));
    auto k_title = new wxStaticText(m_grid_panel, wxID_ANY, _L("Factor K"), wxDefaultPosition, wxDefaultSize, 0);
    k_title->SetFont(Label::Head_14);
    left_title_sizer->Add(k_title, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
    auto n_title = new wxStaticText(m_grid_panel, wxID_ANY, _L("Factor N"), wxDefaultPosition, wxDefaultSize, 0);
    n_title->SetFont(Label::Head_14);
    // hide n value
    n_title->Hide();
    left_title_sizer->Add(n_title, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
    auto brand_title = new wxStaticText(m_grid_panel, wxID_ANY, _L("Name"), wxDefaultPosition, wxDefaultSize, 0);
    brand_title->SetFont(Label::Head_14);
    left_title_sizer->Add(brand_title, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
    grid_sizer->Add(left_title_sizer);
    grid_sizer->AddSpacer(COLUMN_GAP);

    m_is_all_failed = true;
    bool part_failed = false;
    for (auto& item : cali_result) {
        bool result_failed = false;
        if (item.confidence != 0) {
            result_failed = true;
            part_failed = true;
        }
        else {
            m_is_all_failed = false;
        }

        wxBoxSizer* column_data_sizer = new wxBoxSizer(wxVERTICAL);
        auto tray_title = new wxStaticText(m_grid_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0);
        tray_title->SetFont(Label::Head_14);
        wxString tray_name;
        if (item.tray_id == VIRTUAL_TRAY_ID) {
            tray_name = "Ext";
        }
        else {
            char prefix = 'A' + (item.tray_id / 4);
            char suffix = '0' + 1 + item.tray_id % 4;
            tray_name = std::string(1, prefix) + std::string(1, suffix);
        }
        tray_title->SetLabel(tray_name);

        auto k_value = new GridTextInput(m_grid_panel, "", "", CALIBRATION_FROM_TO_INPUT_SIZE, item.tray_id, GridTextInputType::K);
        auto n_value = new GridTextInput(m_grid_panel, "", "", CALIBRATION_FROM_TO_INPUT_SIZE, item.tray_id, GridTextInputType::N);
        k_value->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
        n_value->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
        auto k_value_failed = new wxStaticText(m_grid_panel, wxID_ANY, _L("Failed"), wxDefaultPosition, CALIBRATION_FROM_TO_INPUT_SIZE);
        auto n_value_failed = new wxStaticText(m_grid_panel, wxID_ANY, _L("Failed"), wxDefaultPosition, CALIBRATION_FROM_TO_INPUT_SIZE);

        auto comboBox_tray_name = new GridComboBox(m_grid_panel, CALIBRATION_FROM_TO_INPUT_SIZE, item.tray_id);
        auto tray_name_failed = new wxStaticText(m_grid_panel, wxID_ANY, " - ", wxDefaultPosition, CALIBRATION_FROM_TO_INPUT_SIZE);
        wxArrayString selections;
        static std::vector<PACalibResult> filtered_results;
        filtered_results.clear();
        for (auto history : history_result) {
            for (auto& info : m_obj->selected_cali_preset) {
                if (history.filament_id == info.filament_id) {
                    filtered_results.push_back(history);
                    selections.push_back(history.name);
                }
            }
        }
        comboBox_tray_name->Set(selections);

        column_data_sizer->Add(tray_title, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(k_value, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(n_value, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(k_value_failed, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(n_value_failed, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(comboBox_tray_name, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(tray_name_failed, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);

        auto set_edit_mode = [this, k_value, n_value, k_value_failed, n_value_failed, comboBox_tray_name, tray_name_failed](std::string str) {
            if (str == "normal") {
                comboBox_tray_name->Show();
                tray_name_failed->Show(false);
                k_value->Show();
                n_value->Show();
                k_value_failed->Show(false);
                n_value_failed->Show(false);
            }
            if (str == "failed") {
                comboBox_tray_name->Show(false);
                tray_name_failed->Show();
                k_value->Show(false);
                n_value->Show(false);
                k_value_failed->Show();
                n_value_failed->Show();
            }

            // hide n value
            n_value->Hide();
            n_value_failed->Hide();

            m_grid_panel->Layout();
            m_grid_panel->Update();
        };

        if (!result_failed) {
            set_edit_mode("normal");

            auto k_str = wxString::Format("%.3f", item.k_value);
            auto n_str = wxString::Format("%.3f", item.n_coef);
            k_value->GetTextCtrl()->SetValue(k_str);
            n_value->GetTextCtrl()->SetValue(n_str);
            for (auto& info : m_obj->selected_cali_preset) {
                if (item.tray_id == info.tray_id) {
                    comboBox_tray_name->SetValue(get_default_name(info.name, CalibMode::Calib_PA_Line) + "_" + tray_name);
                    break;
                }
                else {
                    BOOST_LOG_TRIVIAL(trace) << "CaliPASaveAutoPanel : obj->selected_cali_preset doesn't contain correct tray_id";
                }
            }

            comboBox_tray_name->Bind(wxEVT_COMBOBOX, [this, comboBox_tray_name, k_value, n_value](auto& e) {
                int selection = comboBox_tray_name->GetSelection();
                auto history = filtered_results[selection];
                });
        }
        else {
            set_edit_mode("failed");
        }

        grid_sizer->Add(column_data_sizer);
        grid_sizer->AddSpacer(COLUMN_GAP);
    }

    m_grid_panel->SetSizer(grid_sizer, true);
    m_grid_panel->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        SetFocusIgnoringChildren();
        });

    if (part_failed) {
        m_part_failed_panel->Show();
        m_complete_text_panel->Show();
        if (m_is_all_failed) {
            m_complete_text_panel->Hide();
        }
    }
    else {
        m_complete_text_panel->Show();
        m_part_failed_panel->Hide();
    }

    Layout();
}

void CaliPASaveAutoPanel::save_to_result_from_widgets(wxWindow* window, bool* out_is_valid, wxString* out_msg) {
    if (!window)
        return;

    //operate
    auto input = dynamic_cast<GridTextInput*>(window);
    if (input && input->IsShown()) {
        int tray_id = input->get_col_idx();
        if (input->get_type() == GridTextInputType::K) {
            float k = 0.0f;
            if (!CalibUtils::validate_input_k_value(input->GetTextCtrl()->GetValue(), &k)) {
                *out_msg = _L("Please input a valid value (K in 0~0.5)");
                *out_is_valid = false;
            }
            m_calib_results[tray_id].k_value = k;
        }
        else if (input->get_type() == GridTextInputType::N) {
        }
    }

    auto comboBox = dynamic_cast<GridComboBox*>(window);
    if (comboBox && comboBox->IsShown()) {
        int tray_id = comboBox->get_col_idx();
        wxString name = comboBox->GetTextCtrl()->GetValue().ToStdString();
        if (name.IsEmpty()) {
            *out_msg = _L("Please enter the name you want to save to printer.");
            *out_is_valid = false;
        }
        else if (name.Length() > 40) {
            *out_msg = _L("The name cannot exceed 40 characters.");
            *out_is_valid = false;
        }
        m_calib_results[tray_id].name = name.ToStdString();
    }
    
    auto childern = window->GetChildren();
    for (auto child : childern) {
        save_to_result_from_widgets(child, out_is_valid, out_msg);
    }
};

bool CaliPASaveAutoPanel::get_result(std::vector<PACalibResult>& out_result) {
    bool is_valid = true;
    wxString err_msg;
    // Check if the input value is valid and save to m_calib_results
    save_to_result_from_widgets(m_grid_panel, &is_valid, &err_msg);
    if (is_valid) {
        // Check for duplicate names
        struct PACalibResult {
            size_t operator()(const std::pair<std::string ,std::string>& item) const {
                return std::hash<string>()(item.first) * std::hash<string>()(item.second);
            }
        };
        std::unordered_set<std::pair<std::string, std::string>, PACalibResult> set;
        for (auto& result : m_calib_results) {
            if (!set.insert({ result.second.name, result.second.filament_id }).second) {
                MessageDialog msg_dlg(nullptr, _L("Only one of the results with the same name will be saved. Are you sure you want to overrides the other results?"), wxEmptyString, wxICON_WARNING | wxYES_NO);
                if (msg_dlg.ShowModal() != wxID_YES) {
                    return false;
                }
                else {
                    break;
                }
            }
        }
        // Check for duplicate names from history
        for (auto& result : m_history_results) {
            if (!set.insert({ result.name, result.filament_id }).second) {
                MessageDialog msg_dlg(nullptr, wxString::Format(_L("There is already a historical calibration result with the same name: %s. Only one of the results with the same name is saved. Are you sure you want to overrides the historical result?"), result.name), wxEmptyString, wxICON_WARNING | wxYES_NO);
                if (msg_dlg.ShowModal() != wxID_YES) {
                    return false;
                }
            }
        }

        for (auto& result : m_calib_results) {
            out_result.push_back(result.second);
        }
        return true;
    }
    else {
        MessageDialog msg_dlg(nullptr, err_msg, wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
}

CaliPASaveManualPanel::CaliPASaveManualPanel(
    wxWindow* parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : wxPanel(parent, id, pos, size, style)
{
    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_panel(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

void CaliPASaveManualPanel::create_panel(wxWindow* parent)
{
    auto complete_text_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBoxSizer* complete_text_sizer = new wxBoxSizer(wxVERTICAL);
    auto complete_text = new wxStaticText(complete_text_panel, wxID_ANY, _L("Please find the best line on your plate"));
    complete_text->SetFont(Label::Head_14);
    complete_text->Wrap(CALIBRATION_TEXT_MAX_LENGTH);
    complete_text_sizer->Add(complete_text, 0, wxALIGN_CENTER);
    complete_text_panel->SetSizer(complete_text_sizer);
    m_top_sizer->Add(complete_text_panel, 0, wxALIGN_CENTER, 0);

    m_top_sizer->AddSpacer(FromDIP(20));

    m_record_picture = new wxStaticBitmap(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
    m_top_sizer->Add(m_record_picture, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 0);
    set_save_img();

    m_top_sizer->AddSpacer(FromDIP(20));

    auto k_value_text = new wxStaticText(parent, wxID_ANY, _L("Factor K"), wxDefaultPosition, wxDefaultSize, 0);
    k_value_text->SetFont(::Label::Head_14);
    k_value_text->Wrap(-1);
    auto n_value_text = new wxStaticText(parent, wxID_ANY, _L("Factor N"), wxDefaultPosition, wxDefaultSize, 0);
    n_value_text->SetFont(::Label::Head_14);
    n_value_text->Wrap(-1);
    n_value_text->Hide();
    m_k_val = new TextInput(parent, wxEmptyString, "", "", wxDefaultPosition, CALIBRATION_OPTIMAL_INPUT_SIZE, 0);
    m_n_val = new TextInput(parent, wxEmptyString, "", "", wxDefaultPosition, CALIBRATION_OPTIMAL_INPUT_SIZE, 0);
    m_n_val->Hide();
    m_top_sizer->Add(k_value_text, 0);
    m_top_sizer->Add(m_k_val, 0);

    m_top_sizer->AddSpacer(FromDIP(20));

    auto save_text = new wxStaticText(parent, wxID_ANY, _L("Name"), wxDefaultPosition, wxDefaultSize, 0);
    save_text->SetFont(Label::Head_14);
    m_top_sizer->Add(save_text, 0, 0, 0);

    m_save_name_input = new TextInput(parent, "", "", "", wxDefaultPosition, { CALIBRATION_TEXT_MAX_LENGTH, FromDIP(24) }, 0);
    m_top_sizer->Add(m_save_name_input, 0, 0, 0);

    m_top_sizer->AddSpacer(PRESET_GAP);

    wxStaticText* naming_hints = new wxStaticText(parent, wxID_ANY, _L("*We recommend you to add brand, materia, type, and even humidity level in the Name"));
    naming_hints->SetFont(Label::Body_14);
    naming_hints->SetForegroundColour(wxColour(157, 157, 157));
    m_top_sizer->Add(naming_hints, 0, wxALIGN_CENTER, 0);

    m_top_sizer->AddSpacer(FromDIP(20));

    Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        SetFocusIgnoringChildren();
        });
}

void CaliPASaveManualPanel::set_save_img() {
    m_record_picture->SetBitmap(create_scaled_bitmap("fd_calibration_manual_result", nullptr, 400));
}

void CaliPASaveManualPanel::set_default_name(const wxString& name) {
    m_save_name_input->GetTextCtrl()->SetValue(name);
}

bool CaliPASaveManualPanel::get_result(PACalibResult& out_result) {
    // Check if the value is valid
    float k;
    if (!CalibUtils::validate_input_k_value(m_k_val->GetTextCtrl()->GetValue(), &k)) {
        MessageDialog msg_dlg(nullptr, _L("Please input a valid value (K in 0~0.5)"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }

    wxString name = m_save_name_input->GetTextCtrl()->GetValue();
    if (name.IsEmpty()) {
        MessageDialog msg_dlg(nullptr, _L("Please enter the name you want to save to printer."), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
    else if (name.Length() > 40) {
        MessageDialog msg_dlg(nullptr, _L("The name cannot exceed 40 characters."), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }

    out_result.k_value = k;
    out_result.name = name.ToStdString();
    if (m_obj) {
        assert(m_obj->selected_cali_preset.size() <= 1);
        if (!m_obj->selected_cali_preset.empty()) {
            out_result.tray_id = m_obj->selected_cali_preset[0].tray_id;
            out_result.nozzle_diameter = m_obj->selected_cali_preset[0].nozzle_diameter;
            out_result.filament_id = m_obj->selected_cali_preset[0].filament_id;
            out_result.setting_id = m_obj->selected_cali_preset[0].setting_id;
        }
        else {
            BOOST_LOG_TRIVIAL(trace) << "CaliPASaveManual: obj->selected_cali_preset is empty";
            return false;
        }
    }
    else {
        BOOST_LOG_TRIVIAL(trace) << "CaliPASaveManual::get_result(): obj is nullptr";
        return false;
    }

    return true;
}

bool CaliPASaveManualPanel::Show(bool show) {
    if (show) {
        if (m_obj) {
            assert(m_obj->selected_cali_preset.size() <= 1);
            if (!m_obj->selected_cali_preset.empty()) {
                wxString default_name = get_default_name(m_obj->selected_cali_preset[0].name, CalibMode::Calib_PA_Line);
                set_default_name(default_name);
            }
        }
        else {
            BOOST_LOG_TRIVIAL(trace) << "CaliPASaveManual::Show(): obj is nullptr";
        }
    }
    return wxPanel::Show(show);
}

CaliPASaveP1PPanel::CaliPASaveP1PPanel(
    wxWindow* parent,
    wxWindowID id,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : wxPanel(parent, id, pos, size, style)
{
    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_panel(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

void CaliPASaveP1PPanel::create_panel(wxWindow* parent)
{
    auto complete_text_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    wxBoxSizer* complete_text_sizer = new wxBoxSizer(wxVERTICAL);
    auto complete_text = new wxStaticText(complete_text_panel, wxID_ANY, _L("Please find the best line on your plate"));
    complete_text->SetFont(Label::Head_14);
    complete_text->Wrap(CALIBRATION_TEXT_MAX_LENGTH);
    complete_text_sizer->Add(complete_text, 0, wxALIGN_CENTER);
    complete_text_panel->SetSizer(complete_text_sizer);
    m_top_sizer->Add(complete_text_panel, 0, wxALIGN_CENTER, 0);

    m_top_sizer->AddSpacer(FromDIP(20));

    m_record_picture = new wxStaticBitmap(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
    m_top_sizer->Add(m_record_picture, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 0);
    set_save_img();

    m_top_sizer->AddSpacer(FromDIP(20));

    auto value_sizer = new wxBoxSizer(wxHORIZONTAL);
    auto k_value_text = new wxStaticText(parent, wxID_ANY, _L("Factor K"), wxDefaultPosition, wxDefaultSize, 0);
    k_value_text->Wrap(-1);
    k_value_text->SetFont(::Label::Head_14);
    auto n_value_text = new wxStaticText(parent, wxID_ANY, _L("Factor N"), wxDefaultPosition, wxDefaultSize, 0);
    n_value_text->Wrap(-1);
    n_value_text->SetFont(::Label::Head_14);
    m_k_val = new TextInput(parent, wxEmptyString, "", "", wxDefaultPosition, CALIBRATION_OPTIMAL_INPUT_SIZE, 0);
    m_n_val = new TextInput(parent, wxEmptyString, "", "", wxDefaultPosition, CALIBRATION_OPTIMAL_INPUT_SIZE, 0);
    n_value_text->Hide();
    m_n_val->Hide();
    value_sizer->Add(k_value_text, 0, wxALIGN_CENTER_VERTICAL, 0);
    value_sizer->AddSpacer(FromDIP(10));
    value_sizer->Add(m_k_val, 0);
    value_sizer->AddSpacer(FromDIP(50));
    value_sizer->Add(n_value_text, 0, wxALIGN_CENTER_VERTICAL, 0);
    value_sizer->AddSpacer(FromDIP(10));
    value_sizer->Add(m_n_val, 0);
    m_top_sizer->Add(value_sizer, 0, wxALIGN_CENTER);

    Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        SetFocusIgnoringChildren();
        });
}

void CaliPASaveP1PPanel::set_save_img() {
    m_record_picture->SetBitmap(create_scaled_bitmap("fd_calibration_manual_result", nullptr, 400));
}

bool CaliPASaveP1PPanel::get_result(float* out_k, float* out_n){
    // Check if the value is valid
    if (!CalibUtils::validate_input_k_value(m_k_val->GetTextCtrl()->GetValue(), out_k)) {
        MessageDialog msg_dlg(nullptr, _L("Please input a valid value (K in 0~0.5)"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
    return true;
}

CaliSavePresetValuePanel::CaliSavePresetValuePanel(
    wxWindow *parent,
    wxWindowID id,
    const wxPoint &pos,
    const wxSize &size,
    long style)
    : wxPanel(parent, id, pos, size, style)
{
    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_panel(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

void CaliSavePresetValuePanel::create_panel(wxWindow *parent)
{
    m_record_picture = new wxStaticBitmap(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);

    m_value_title = new wxStaticText(parent, wxID_ANY, _L("Input Value"), wxDefaultPosition, wxDefaultSize, 0);
    m_value_title->SetFont(Label::Head_14);
    m_value_title->Wrap(-1);
    m_input_value = new TextInput(parent, wxEmptyString, "", "", wxDefaultPosition, CALIBRATION_OPTIMAL_INPUT_SIZE, wxTE_PROCESS_ENTER);
    m_input_value->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

    m_save_name_title = new wxStaticText(parent, wxID_ANY, _L("Save to Filament Preset"), wxDefaultPosition, wxDefaultSize, 0);
    m_save_name_title->Wrap(-1);
    m_save_name_title->SetFont(Label::Head_14);

    m_input_name = new TextInput(parent, wxEmptyString, "", "", wxDefaultPosition, {CALIBRATION_TEXT_MAX_LENGTH, FromDIP(24)}, 0);

    m_top_sizer->Add(m_record_picture, 0, wxALIGN_CENTER);
    m_top_sizer->AddSpacer(FromDIP(20));
    m_top_sizer->Add(m_value_title, 0);
    m_top_sizer->AddSpacer(FromDIP(10));
    m_top_sizer->Add(m_input_value, 0);
    m_top_sizer->AddSpacer(FromDIP(20));
    m_top_sizer->Add(m_save_name_title, 0);
    m_top_sizer->AddSpacer(FromDIP(10));
    m_top_sizer->Add(m_input_name, 0);
    m_top_sizer->AddSpacer(FromDIP(20));
}

void CaliSavePresetValuePanel::set_img(const std::string& bmp_name_in)
{
    m_record_picture->SetBitmap(create_scaled_bitmap(bmp_name_in, nullptr, 400));
}

void CaliSavePresetValuePanel::set_value_title(const wxString& title) {
    m_value_title->SetLabel(title);
}

void CaliSavePresetValuePanel::set_save_name_title(const wxString& title) {
    m_save_name_title->SetLabel(title);
}

void CaliSavePresetValuePanel::get_value(double& value) 
{ 
    m_input_value->GetTextCtrl()->GetValue().ToDouble(&value); 
}

void CaliSavePresetValuePanel::get_save_name(std::string& name)
{ 
    name = m_input_name->GetTextCtrl()->GetValue().ToStdString(); 
}

void CaliSavePresetValuePanel::set_save_name(const std::string& name)
{ 
    m_input_name->GetTextCtrl()->SetValue(name); 
}

CalibrationPASavePage::CalibrationPASavePage(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : CalibrationCommonSavePage(parent, id, pos, size, style)
{
    m_cali_mode = CalibMode::Calib_PA_Line;

    m_page_type = CaliPageType::CALI_PAGE_PA_SAVE;

    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_page(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

void CalibrationPASavePage::create_page(wxWindow* parent)
{
    m_page_caption = new CaliPageCaption(parent, m_cali_mode);
    m_page_caption->show_prev_btn(true);
    m_top_sizer->Add(m_page_caption, 0, wxEXPAND, 0);

    wxArrayString steps;
    steps.Add(_L("Preset"));
    steps.Add(_L("Calibration"));
    steps.Add(_L("Record"));
    m_step_panel = new CaliPageStepGuide(parent, steps);
    m_step_panel->set_steps(2);
    m_top_sizer->Add(m_step_panel, 0, wxEXPAND, 0);

    m_manual_panel = new CaliPASaveManualPanel(parent, wxID_ANY);
    m_auto_panel = new CaliPASaveAutoPanel(parent, wxID_ANY);
    m_p1p_panel = new CaliPASaveP1PPanel(parent, wxID_ANY);

    m_top_sizer->Add(m_manual_panel, 0, wxEXPAND);
    m_top_sizer->Add(m_auto_panel, 0, wxEXPAND);
    m_top_sizer->Add(m_p1p_panel, 0, wxEXPAND);

    m_action_panel = new CaliPageActionPanel(parent, m_cali_mode, CaliPageType::CALI_PAGE_PA_SAVE);
    m_top_sizer->Add(m_action_panel, 0, wxEXPAND, 0);
}

void CalibrationPASavePage::sync_cali_result(MachineObject* obj)
{
    // only auto need sync cali_result
    if (obj && m_cali_method == CalibrationMethod::CALI_METHOD_AUTO) {
        m_auto_panel->sync_cali_result(obj->pa_calib_results, obj->pa_calib_tab);
    } else {
        std::vector<PACalibResult> empty_result;
        m_auto_panel->sync_cali_result(empty_result, empty_result);
    }
}

void CalibrationPASavePage::show_panels(CalibrationMethod method, const PrinterSeries printer_ser) {
    if (printer_ser == PrinterSeries::SERIES_X1) {
        if (method == CalibrationMethod::CALI_METHOD_MANUAL) {
            m_manual_panel->Show();
            m_auto_panel->Show(false);
        }
        else {
            m_auto_panel->Show();
            m_manual_panel->Show(false);
        }
        m_p1p_panel->Show(false);
    }
    else if (printer_ser == PrinterSeries::SERIES_P1P) {
        m_auto_panel->Show(false);
        m_manual_panel->Show(false);
        m_p1p_panel->Show();
    } else {
        m_auto_panel->Show(false);
        m_manual_panel->Show(false);
        m_p1p_panel->Show();
        assert(false);
    }
    Layout();
}

void CalibrationPASavePage::set_cali_method(CalibrationMethod method)
{
    CalibrationWizardPage::set_cali_method(method);
    if (curr_obj) {
        show_panels(method, curr_obj->get_printer_series());
    }
}

void CalibrationPASavePage::on_device_connected(MachineObject* obj)
{
    curr_obj = obj;
    if (curr_obj)
        show_panels(m_cali_method, curr_obj->get_printer_series());
}

void CalibrationPASavePage::update(MachineObject* obj)
{
    CalibrationWizardPage::update(obj);

    if (m_auto_panel && m_auto_panel->IsShown())
        m_auto_panel->set_machine_obj(obj);
    if (m_manual_panel && m_manual_panel->IsShown())
        m_manual_panel->set_machine_obj(obj);
}

bool CalibrationPASavePage::Show(bool show) {
    if (show) {
        if (curr_obj) {
            show_panels(m_cali_method, curr_obj->get_printer_series());
            sync_cali_result(curr_obj);
        }
    }
    return wxPanel::Show(show);
}

CalibrationFlowX1SavePage::CalibrationFlowX1SavePage(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : CalibrationCommonSavePage(parent, id, pos, size, style)
{
    m_cali_mode = CalibMode::Calib_Flow_Rate;

    m_page_type = CaliPageType::CALI_PAGE_FLOW_SAVE;

    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_page(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

void CalibrationFlowX1SavePage::create_page(wxWindow* parent)
{
    m_page_caption = new CaliPageCaption(parent, m_cali_mode);
    m_page_caption->show_prev_btn(true);
    m_top_sizer->Add(m_page_caption, 0, wxEXPAND, 0);

    wxArrayString steps;
    steps.Add(_L("Preset"));
    steps.Add(_L("Calibration"));
    steps.Add(_L("Record"));
    m_step_panel = new CaliPageStepGuide(parent, steps);
    m_step_panel->set_steps(2);
    m_top_sizer->Add(m_step_panel, 0, wxEXPAND, 0);

    m_complete_text_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_complete_text_panel->Hide();
    wxBoxSizer* complete_text_sizer = new wxBoxSizer(wxVERTICAL);
    auto complete_text = new wxStaticText(m_complete_text_panel, wxID_ANY, _L("We found the best flow ratio for you"));
    complete_text->SetFont(Label::Head_14);
    complete_text->Wrap(CALIBRATION_TEXT_MAX_LENGTH);
    complete_text_sizer->Add(complete_text, 0, wxALIGN_CENTER);
    m_complete_text_panel->SetSizer(complete_text_sizer);

    m_part_failed_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_part_failed_panel->SetBackgroundColour(wxColour(238, 238, 238));
    wxBoxSizer* part_failed_sizer = new wxBoxSizer(wxVERTICAL);
    m_part_failed_panel->SetSizer(part_failed_sizer);
    part_failed_sizer->AddSpacer(FromDIP(10));
    auto part_failed_text = new wxStaticText(m_part_failed_panel, wxID_ANY, _L("Part of the calibration failed! You may clean the plate and retry. The failed test result would be droped."));
    part_failed_text->SetFont(Label::Body_14);
    part_failed_text->Wrap(CALIBRATION_TEXT_MAX_LENGTH);
    part_failed_sizer->Add(part_failed_text, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, FromDIP(20));
    part_failed_sizer->AddSpacer(FromDIP(10));

    m_top_sizer->Add(m_complete_text_panel, 0, wxALIGN_CENTER, 0);

    m_top_sizer->Add(m_part_failed_panel, 0, wxALIGN_CENTER, 0);

    m_top_sizer->AddSpacer(FromDIP(20));

    m_grid_panel = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
    m_top_sizer->Add(m_grid_panel, 0, wxALIGN_CENTER);

    m_action_panel = new CaliPageActionPanel(parent, m_cali_mode, CaliPageType::CALI_PAGE_FLOW_SAVE);
    m_top_sizer->Add(m_action_panel, 0, wxEXPAND, 0);
}

void CalibrationFlowX1SavePage::sync_cali_result(const std::vector<FlowRatioCalibResult>& cali_result)
{
    m_save_results.clear();
    m_grid_panel->DestroyChildren();
    wxBoxSizer* grid_sizer = new wxBoxSizer(wxHORIZONTAL);
    const int COLUMN_GAP = FromDIP(50);
    const int ROW_GAP = FromDIP(30);
    wxBoxSizer* left_title_sizer = new wxBoxSizer(wxVERTICAL);
    left_title_sizer->AddSpacer(FromDIP(49));
    auto flow_ratio_title = new wxStaticText(m_grid_panel, wxID_ANY, _L("Flow Ratio"), wxDefaultPosition, wxDefaultSize, 0);
    flow_ratio_title->SetFont(Label::Head_14);
    left_title_sizer->Add(flow_ratio_title, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
    auto brand_title = new wxStaticText(m_grid_panel, wxID_ANY, _L("Save to Filament Preset"), wxDefaultPosition, wxDefaultSize, 0);
    brand_title->SetFont(Label::Head_14);
    left_title_sizer->Add(brand_title, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
    grid_sizer->Add(left_title_sizer);
    grid_sizer->AddSpacer(COLUMN_GAP);

    m_is_all_failed = true;
    bool part_failed = false;

    for (auto& item : cali_result) {
        bool result_failed = false;
        if (item.confidence != 0) {
            result_failed = true;
            part_failed = true;
        }
        else {
            m_is_all_failed = false;
        }

        wxBoxSizer* column_data_sizer = new wxBoxSizer(wxVERTICAL);
        auto tray_title = new wxStaticText(m_grid_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0);
        tray_title->SetFont(Label::Head_14);
        wxString tray_name;
        if (item.tray_id == VIRTUAL_TRAY_ID) {
            tray_name = "Ext";
        }
        else {
            char prefix = 'A' + (item.tray_id / 4);
            char suffix = '0' + 1 + item.tray_id % 4;
            tray_name = std::string(1, prefix) + std::string(1, suffix);
        }
        tray_title->SetLabel(tray_name);

        auto flow_ratio_value = new GridTextInput(m_grid_panel, "", "", CALIBRATION_FROM_TO_INPUT_SIZE, item.tray_id, GridTextInputType::FlowRatio);
        flow_ratio_value->GetTextCtrl()->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
        auto flow_ratio_value_failed = new wxStaticText(m_grid_panel, wxID_ANY, _L("Failed"), wxDefaultPosition, CALIBRATION_FROM_TO_INPUT_SIZE);

        auto save_name_input = new GridTextInput(m_grid_panel, "", "", CALIBRATION_FROM_TO_INPUT_SIZE, item.tray_id, GridTextInputType::Name);
        auto save_name_input_failed = new wxStaticText(m_grid_panel, wxID_ANY, " - ", wxDefaultPosition, CALIBRATION_FROM_TO_INPUT_SIZE);

        column_data_sizer->Add(tray_title, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(flow_ratio_value, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(flow_ratio_value_failed, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(save_name_input, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);
        column_data_sizer->Add(save_name_input_failed, 0, wxALIGN_CENTER | wxBOTTOM, ROW_GAP);

        auto set_edit_mode = [this, flow_ratio_value, flow_ratio_value_failed, save_name_input, save_name_input_failed](std::string str) {
            if (str == "normal") {
                save_name_input->Show();
                save_name_input_failed->Show(false);
                flow_ratio_value->Show();
                flow_ratio_value_failed->Show(false);
            }
            if (str == "failed") {
                save_name_input->Show(false);
                save_name_input_failed->Show();
                flow_ratio_value->Show(false);
                flow_ratio_value_failed->Show();
            }
            m_grid_panel->Layout();
            m_grid_panel->Update();
        };

        if (!result_failed) {
            set_edit_mode("normal");

            auto flow_ratio_str = wxString::Format("%.3f", item.flow_ratio);
            flow_ratio_value->GetTextCtrl()->SetValue(flow_ratio_str);
            for (auto& info : curr_obj->selected_cali_preset) {
                if (item.tray_id == info.tray_id) {
                    save_name_input->GetTextCtrl()->SetValue(get_default_name(info.name, CalibMode::Calib_Flow_Rate) + "_" + tray_name);
                    break;
                }
                else {
                    BOOST_LOG_TRIVIAL(trace) << "CalibrationFlowX1Save : obj->selected_cali_preset doesn't contain correct tray_id";
                }
            }
        }
        else {
            set_edit_mode("failed");
        }

        grid_sizer->Add(column_data_sizer);
        grid_sizer->AddSpacer(COLUMN_GAP);
    }
    m_grid_panel->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        m_grid_panel->SetFocusIgnoringChildren();
        });
    m_grid_panel->SetSizer(grid_sizer, true);

    if (part_failed) {
        m_part_failed_panel->Show();
        m_complete_text_panel->Show();
        if (m_is_all_failed) {
            m_complete_text_panel->Hide();
        }
    }
    else {
        m_complete_text_panel->Show();
        m_part_failed_panel->Hide();
    }

    Layout();
}

void CalibrationFlowX1SavePage::save_to_result_from_widgets(wxWindow* window, bool* out_is_valid, wxString* out_msg)
{
    if (!window)
        return;

    //operate
    auto input = dynamic_cast<GridTextInput*>(window);
    if (input && input->IsShown()) {
        int tray_id = input->get_col_idx();
        if (input->get_type() == GridTextInputType::FlowRatio) {
            float flow_ratio = 0.0f;
            if (!CalibUtils::validate_input_flow_ratio(input->GetTextCtrl()->GetValue(), &flow_ratio)) {
                *out_msg = _L("Please input a valid value (0.0 < flow ratio < 2.0)");
                *out_is_valid = false;
            }
            m_save_results[tray_id].second = flow_ratio;
        }
        else if (input->get_type() == GridTextInputType::Name) {
            if (input->GetTextCtrl()->GetValue().IsEmpty()) {
                *out_msg = _L("Please enter the name of the preset you want to save.");
                *out_is_valid = false;
            }
            m_save_results[tray_id].first = input->GetTextCtrl()->GetValue().ToStdString();
        }
    }

    auto childern = window->GetChildren();
    for (auto child : childern) {
        save_to_result_from_widgets(child, out_is_valid, out_msg);
    }
}

bool CalibrationFlowX1SavePage::get_result(std::vector<std::pair<wxString, float>>& out_results)
{
    bool is_valid = true;
    wxString err_msg;
    // Check if the value is valid and save to m_calib_results
    save_to_result_from_widgets(m_grid_panel, &is_valid, &err_msg);
    if (is_valid) {
        // obj->cali_result contain failure results, so use m_save_results to record value
        for (auto& item : m_save_results) {
            out_results.push_back(item.second);
        }
        return true;
    }
    else {
        MessageDialog msg_dlg(nullptr, err_msg, wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
}

bool CalibrationFlowX1SavePage::Show(bool show) {
    if (show) {
        if (curr_obj) {
            sync_cali_result(curr_obj->flow_ratio_results);
        }
    }
    return wxPanel::Show(show);
}

CalibrationFlowCoarseSavePage::CalibrationFlowCoarseSavePage(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : CalibrationCommonSavePage(parent, id, pos, size, style)
{
    m_cali_mode = CalibMode::Calib_Flow_Rate;

    m_page_type = CaliPageType::CALI_PAGE_COARSE_SAVE;

    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_page(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

void CalibrationFlowCoarseSavePage::create_page(wxWindow* parent)
{
    m_page_caption = new CaliPageCaption(parent, m_cali_mode);
    m_page_caption->show_prev_btn(true);
    m_top_sizer->Add(m_page_caption, 0, wxEXPAND, 0);

    wxArrayString steps;
    steps.Add(_L("Preset"));
    steps.Add(_L("Calibration1"));
    steps.Add(_L("Calibration2"));
    steps.Add(_L("Record"));
    m_step_panel = new CaliPageStepGuide(parent, steps);
    m_step_panel->set_steps(1);
    m_top_sizer->Add(m_step_panel, 0, wxEXPAND, 0);

    auto complete_text = new wxStaticText(parent, wxID_ANY, _L("Please find the best object on your plate"), wxDefaultPosition, wxDefaultSize, 0);
    complete_text->SetFont(Label::Head_14);
    complete_text->Wrap(-1);
    m_top_sizer->Add(complete_text, 0, wxALIGN_CENTER, 0);
    m_top_sizer->AddSpacer(FromDIP(20));

    m_record_picture = new wxStaticBitmap(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
    m_top_sizer->Add(m_record_picture, 0, wxALIGN_CENTER, 0);
    set_save_img();

    m_top_sizer->AddSpacer(FromDIP(20));

    auto coarse_value_sizer = new wxBoxSizer(wxVERTICAL);
    auto coarse_value_text = new wxStaticText(parent, wxID_ANY, _L("Fill in the value above the block with smoothest top surface"), wxDefaultPosition, wxDefaultSize, 0);
    coarse_value_text->SetFont(Label::Head_14);
    coarse_value_text->Wrap(-1);
    m_optimal_block_coarse = new ComboBox(parent, wxID_ANY, "", wxDefaultPosition, CALIBRATION_OPTIMAL_INPUT_SIZE, 0, nullptr, wxCB_READONLY);
    wxArrayString coarse_block_items;
    for (int i = 0; i < 9; i++) {
        coarse_block_items.Add(std::to_string(-20 + (i * 5)));
    }
    m_optimal_block_coarse->Set(coarse_block_items);
    auto coarse_calc_result_text = new wxStaticText(parent, wxID_ANY, "");
    coarse_value_sizer->Add(coarse_value_text, 0, 0);
    coarse_value_sizer->Add(m_optimal_block_coarse, 0, 0);
    coarse_value_sizer->Add(coarse_calc_result_text, 0);
    m_top_sizer->Add(coarse_value_sizer, 0, 0, 0);
    m_top_sizer->AddSpacer(FromDIP(20));

    auto checkBox_panel = new wxPanel(parent);
    auto cb_sizer = new wxBoxSizer(wxHORIZONTAL);
    checkBox_panel->SetSizer(cb_sizer);
    auto checkBox_skip_calibration = new CheckBox(checkBox_panel);
    cb_sizer->Add(checkBox_skip_calibration);

    auto cb_text = new wxStaticText(checkBox_panel, wxID_ANY, _L("Skip Calibration2"));
    cb_sizer->Add(cb_text);
    cb_text->Bind(wxEVT_LEFT_DOWN, [this, checkBox_skip_calibration](auto&) {
        checkBox_skip_calibration->SetValue(!checkBox_skip_calibration->GetValue());
        wxCommandEvent event(wxEVT_TOGGLEBUTTON);
        event.SetEventObject(checkBox_skip_calibration);
        checkBox_skip_calibration->GetEventHandler()->ProcessEvent(event);
        });

    m_top_sizer->Add(checkBox_panel, 0, 0, 0);

    auto save_panel = new wxPanel(parent);
    auto save_sizer = new wxBoxSizer(wxVERTICAL);
    save_panel->SetSizer(save_sizer);

    auto save_text = new wxStaticText(save_panel, wxID_ANY, _L("Save to Filament Preset"), wxDefaultPosition, wxDefaultSize, 0);
    save_text->Wrap(-1);
    save_text->SetFont(Label::Head_14);
    save_sizer->Add(save_text, 0, 0, 0);

    m_save_name_input = new TextInput(save_panel, "", "", "", wxDefaultPosition, {CALIBRATION_TEXT_MAX_LENGTH, FromDIP(24)}, 0);
    save_sizer->Add(m_save_name_input, 0, 0, 0);

    m_top_sizer->Add(save_panel, 0, 0, 0);
    save_panel->Hide();

    m_top_sizer->AddSpacer(FromDIP(20));

    checkBox_skip_calibration->Bind(wxEVT_TOGGLEBUTTON, [this, save_panel, checkBox_skip_calibration](wxCommandEvent& e) {
        if (checkBox_skip_calibration->GetValue()) {
            m_skip_fine_calibration = true;
            save_panel->Show();
            m_action_panel->show_button(CaliPageActionType::CALI_ACTION_FLOW_COARSE_SAVE);
            m_action_panel->show_button(CaliPageActionType::CALI_ACTION_FLOW_CALI_STAGE_2, false);
        }
        else {
            m_skip_fine_calibration = false;
            save_panel->Hide();
            m_action_panel->show_button(CaliPageActionType::CALI_ACTION_FLOW_COARSE_SAVE, false);
            m_action_panel->show_button(CaliPageActionType::CALI_ACTION_FLOW_CALI_STAGE_2);
        }
        Layout();
        e.Skip();
        });

    m_optimal_block_coarse->Bind(wxEVT_COMBOBOX, [this, coarse_calc_result_text](auto& e) {
        m_coarse_flow_ratio = m_curr_flow_ratio * (100.0f + stof(m_optimal_block_coarse->GetValue().ToStdString())) / 100.0f;
        coarse_calc_result_text->SetLabel(wxString::Format(_L("flow ratio : %s "), std::to_string(m_coarse_flow_ratio)));
        });

    m_action_panel = new CaliPageActionPanel(parent, m_cali_mode, CaliPageType::CALI_PAGE_COARSE_SAVE);
    m_action_panel->show_button(CaliPageActionType::CALI_ACTION_FLOW_COARSE_SAVE, false);
    m_top_sizer->Add(m_action_panel, 0, wxEXPAND, 0);
}

void CalibrationFlowCoarseSavePage::set_save_img() {
    m_record_picture->SetBitmap(create_scaled_bitmap("flow_rate_calibration_coarse_result", nullptr, 400));
}

void CalibrationFlowCoarseSavePage::set_default_name(const wxString& name) {
    m_save_name_input->GetTextCtrl()->SetValue(name);
}

bool CalibrationFlowCoarseSavePage::is_skip_fine_calibration() {
    return m_skip_fine_calibration;
}

void CalibrationFlowCoarseSavePage::set_curr_flow_ratio(const float value) {
    m_curr_flow_ratio = value;
}

bool CalibrationFlowCoarseSavePage::get_result(float* out_value, wxString* out_name) {
    // Check if the value is valid
    if (m_coarse_flow_ratio <= 0.0 || m_coarse_flow_ratio >= 2.0) {
        MessageDialog msg_dlg(nullptr, _L("Please choose a block with smoothest top surface"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
    if (m_save_name_input->GetTextCtrl()->GetValue().IsEmpty()) {
        MessageDialog msg_dlg(nullptr, _L("Please enter the name of the preset you want to save."), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
    *out_value = m_coarse_flow_ratio;
    *out_name = m_save_name_input->GetTextCtrl()->GetValue();
    return true;
}

bool CalibrationFlowCoarseSavePage::Show(bool show) {
    if (show) {
        if (curr_obj) {
            assert(curr_obj->selected_cali_preset.size() <= 1);
            if (!curr_obj->selected_cali_preset.empty()) {
                wxString default_name = get_default_name(curr_obj->selected_cali_preset[0].name, CalibMode::Calib_Flow_Rate);
                set_default_name(default_name);
                set_curr_flow_ratio(curr_obj->cache_flow_ratio);
            }
        }
        else {
            BOOST_LOG_TRIVIAL(trace) << "CalibrationFlowCoarseSave::Show(): obj is nullptr";
        }
    }
    return wxPanel::Show(show);
}

CalibrationFlowFineSavePage::CalibrationFlowFineSavePage(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
    : CalibrationCommonSavePage(parent, id, pos, size, style)
{
    m_cali_mode = CalibMode::Calib_Flow_Rate;

    m_page_type = CaliPageType::CALI_PAGE_FINE_SAVE;

    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_page(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

void CalibrationFlowFineSavePage::create_page(wxWindow* parent)
{
    m_page_caption = new CaliPageCaption(parent, m_cali_mode);
    m_page_caption->show_prev_btn(true);
    m_top_sizer->Add(m_page_caption, 0, wxEXPAND, 0);

    wxArrayString steps;
    steps.Add(_L("Preset"));
    steps.Add(_L("Calibration1"));
    steps.Add(_L("Calibration2"));
    steps.Add(_L("Record"));
    m_step_panel = new CaliPageStepGuide(parent, steps);
    m_step_panel->set_steps(3);
    m_top_sizer->Add(m_step_panel, 0, wxEXPAND, 0);

    auto complete_text = new wxStaticText(parent, wxID_ANY, _L("Please find the best object on your plate"), wxDefaultPosition, wxDefaultSize, 0);
    complete_text->SetFont(Label::Head_14);
    complete_text->Wrap(-1);
    m_top_sizer->Add(complete_text, 0, wxALIGN_CENTER, 0);
    m_top_sizer->AddSpacer(FromDIP(20));

    m_record_picture = new wxStaticBitmap(parent, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxDefaultSize, 0);
    m_top_sizer->Add(m_record_picture, 0, wxALIGN_CENTER, 0);
    set_save_img();

    m_top_sizer->AddSpacer(FromDIP(20));

    auto fine_value_sizer = new wxBoxSizer(wxVERTICAL);
    auto fine_value_text = new wxStaticText(parent, wxID_ANY, _L("Fill in the value above the block with smoothest top surface"), wxDefaultPosition, wxDefaultSize, 0);
    fine_value_text->Wrap(-1);
    fine_value_text->SetFont(::Label::Head_14);
    m_optimal_block_fine = new ComboBox(parent, wxID_ANY, "", wxDefaultPosition, CALIBRATION_OPTIMAL_INPUT_SIZE, 0, nullptr, wxCB_READONLY);
    wxArrayString fine_block_items;
    for (int i = 0; i < 10; i++) {
        fine_block_items.Add(std::to_string(-9 + (i)));
    }
    m_optimal_block_fine->Set(fine_block_items);
    auto fine_calc_result_text = new wxStaticText(parent, wxID_ANY, "");
    fine_value_sizer->Add(fine_value_text, 0, 0);
    fine_value_sizer->Add(m_optimal_block_fine, 0, 0);
    fine_value_sizer->Add(fine_calc_result_text, 0);
    m_top_sizer->Add(fine_value_sizer, 0, 0, 0);
    m_top_sizer->AddSpacer(FromDIP(20));

    auto save_text = new wxStaticText(parent, wxID_ANY, _L("Save to Filament Preset"), wxDefaultPosition, wxDefaultSize, 0);
    save_text->Wrap(-1);
    save_text->SetFont(Label::Head_14);
    m_top_sizer->Add(save_text, 0, 0, 0);

    m_save_name_input = new TextInput(parent, "", "", "", wxDefaultPosition, {CALIBRATION_TEXT_MAX_LENGTH, FromDIP(24)}, 0);
    m_top_sizer->Add(m_save_name_input, 0, 0, 0);

    m_top_sizer->AddSpacer(FromDIP(20));

    m_optimal_block_fine->Bind(wxEVT_COMBOBOX, [this, fine_calc_result_text](auto& e) {
        m_fine_flow_ratio = m_curr_flow_ratio * (100.0f + stof(m_optimal_block_fine->GetValue().ToStdString())) / 100.0f;
        fine_calc_result_text->SetLabel(wxString::Format(_L("flow ratio : %s "), std::to_string(m_fine_flow_ratio)));
        });

    m_action_panel = new CaliPageActionPanel(parent, m_cali_mode, CaliPageType::CALI_PAGE_FINE_SAVE);
    m_top_sizer->Add(m_action_panel, 0, wxEXPAND, 0);
}

void CalibrationFlowFineSavePage::set_save_img() {
    m_record_picture->SetBitmap(create_scaled_bitmap("flow_rate_calibration_fine_result", nullptr, 400));
}

void CalibrationFlowFineSavePage::set_default_name(const wxString& name) {
    m_save_name_input->GetTextCtrl()->SetValue(name);
}

void CalibrationFlowFineSavePage::set_curr_flow_ratio(const float value) {
    m_curr_flow_ratio = value;
}

bool CalibrationFlowFineSavePage::get_result(float* out_value, wxString* out_name) {
    // Check if the value is valid
    if (m_fine_flow_ratio <= 0.0 || m_fine_flow_ratio >= 2.0) {
        MessageDialog msg_dlg(nullptr, _L("Please choose a block with smoothest top surface."), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
    if (m_save_name_input->GetTextCtrl()->GetValue().IsEmpty()) {
        MessageDialog msg_dlg(nullptr, _L("Please enter the name of the preset you want to save."), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
    *out_value = m_fine_flow_ratio;
    *out_name = m_save_name_input->GetTextCtrl()->GetValue();
    return true;
}

bool CalibrationFlowFineSavePage::Show(bool show) {
    if (show) {
        if (curr_obj) {
            assert(curr_obj->selected_cali_preset.size() <= 1);
            if (!curr_obj->selected_cali_preset.empty()) {
                wxString default_name = get_default_name(curr_obj->selected_cali_preset[0].name, CalibMode::Calib_Flow_Rate);
                set_default_name(default_name);
                set_curr_flow_ratio(curr_obj->cache_flow_ratio);
            }
        }
        else {
            BOOST_LOG_TRIVIAL(trace) << "CalibrationFlowFineSave::Show(): obj is nullptr";
        }
    }
    return wxPanel::Show(show);
}

CalibrationMaxVolumetricSpeedSavePage::CalibrationMaxVolumetricSpeedSavePage(
    wxWindow *parent,
    wxWindowID id,
    const wxPoint &pos,
    const wxSize &size,
    long style)
    : CalibrationCommonSavePage(parent, id, pos, size, style)
{
    m_cali_mode = CalibMode::Calib_Vol_speed_Tower;

    m_page_type = CaliPageType::CALI_PAGE_COMMON_SAVE;

    m_top_sizer = new wxBoxSizer(wxVERTICAL);

    create_page(this);

    this->SetSizer(m_top_sizer);
    m_top_sizer->Fit(this);
}

void CalibrationMaxVolumetricSpeedSavePage::create_page(wxWindow *parent)
{
    m_page_caption = new CaliPageCaption(parent, m_cali_mode);
    m_page_caption->show_prev_btn(true);
    m_top_sizer->Add(m_page_caption, 0, wxEXPAND, 0);

    wxArrayString steps;
    steps.Add(_L("Preset"));
    steps.Add(_L("Calibration"));
    steps.Add(_L("Record"));
    m_step_panel = new CaliPageStepGuide(parent, steps);
    m_step_panel->set_steps(2);
    m_top_sizer->Add(m_step_panel, 0, wxEXPAND, 0);

    m_save_preset_panel = new CaliSavePresetValuePanel(parent, wxID_ANY);

    set_save_img();

    m_top_sizer->Add(m_save_preset_panel, 0, wxEXPAND);

    m_action_panel = new CaliPageActionPanel(parent, m_cali_mode, CaliPageType::CALI_PAGE_COMMON_SAVE);
    m_top_sizer->Add(m_action_panel, 0, wxEXPAND, 0);
}

void CalibrationMaxVolumetricSpeedSavePage::set_save_img() {
    m_save_preset_panel->set_img("max_volumetric_speed_calibration");
}

bool CalibrationMaxVolumetricSpeedSavePage::get_save_result(double& value, std::string& name) {
    // Check if the value is valid
    m_save_preset_panel->get_save_name(name);
    m_save_preset_panel->get_value(value);
    if (value < 0 || value > 60) {
        MessageDialog msg_dlg(nullptr, _L("Please input a valid value (0 <= Max Volumetric Speed <= 60)"), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
    if (name.empty()) {
        MessageDialog msg_dlg(nullptr, _L("Please enter the name of the preset you want to save."), wxEmptyString, wxICON_WARNING | wxOK);
        msg_dlg.ShowModal();
        return false;
    }
    return true;
}

bool CalibrationMaxVolumetricSpeedSavePage::Show(bool show) {
    if (show) {
        if (curr_obj) {
            assert(curr_obj->selected_cali_preset.size() <= 1);
            if (!curr_obj->selected_cali_preset.empty()) {
                wxString default_name = get_default_name(curr_obj->selected_cali_preset[0].name, CalibMode::Calib_Vol_speed_Tower);
                m_save_preset_panel->set_save_name(default_name.ToStdString());
            }
        }
        else {
            BOOST_LOG_TRIVIAL(trace) << "CalibrationMaxVolumetricSpeedSave::Show(): obj is nullptr";
        }
    }
    return wxPanel::Show(show);
}


}}
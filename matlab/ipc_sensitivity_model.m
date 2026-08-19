%% ipc_sensitivity_model.m
% Week 1/2 starter analytical model for Project 5.
% Do NOT present the model as validated until measured simulator data exists.
% The purpose in Weeks 1-2 is to define inputs, sweep configurations, and
% make the comparison reproducible.

clear; clc; close all;

%% Model inputs
% These values are configuration inputs, not measured results.
% Baseline project configuration: 3 ADD/SUB RS and 3 MUL/DIV RS.
rsCounts = 1:6;
cdbWidths = 1:3;

% Placeholder kernel-level parameters. Replace with values measured from
% the generated team's kernel once the C simulator is available.
instructionCount = NaN;
serialFraction = NaN;

% Measured data file expected later (Week 3/4):
% results/processed/ipc_measurements.csv
% Required columns: rs_count,cdb_width,measured_ipc
measuredFile = fullfile('..','results','processed','ipc_measurements.csv');

%% Guard: Week 1/2 should run even before measured data exists
if isfile(measuredFile)
    measured = readtable(measuredFile);
    disp('Measured simulator data found:');
    disp(measured);
else
    measured = table();
    disp('No measured IPC file yet. This is expected in Weeks 1-2.');
end

%% Analytical placeholder
% A defensible model should be fitted/validated against the team's measured
% data rather than invented to match it. Until those data exist, leave the
% model output undefined and only verify the input/sweep structure.
modelIPC = NaN(length(rsCounts), length(cdbWidths));

%% Configuration summary
fprintf('RS counts swept: %s\n', mat2str(rsCounts));
fprintf('CDB widths swept: %s\n', mat2str(cdbWidths));
fprintf('Instruction count: %g\n', instructionCount);
fprintf('Serial fraction: %g\n', serialFraction);

%% Week 2 handoff notes
% 1. Run the seeded kernel with each reservation-station configuration.
% 2. Record total cycles and completed instructions from the C simulator.
% 3. Compute measured IPC = completed_instructions / total_cycles.
% 4. Record CDB width for each run.
% 5. Only after measured points exist should the analytical equation be
%    chosen/fitted and plotted against those points.
